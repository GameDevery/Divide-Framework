/*
Copyright (c) 2018 DIVIDE-Studio
Copyright (c) 2009 Ionut Cava

This file is part of DIVIDE Framework.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software
and associated documentation files (the "Software"), to deal in the Software
without restriction,
including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED,
INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
IN CONNECTION WITH THE SOFTWARE
OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

#ifndef _COMMAND_BUFFER_INL_
#define _COMMAND_BUFFER_INL_


namespace Divide {
namespace GFX {

template <typename T, CommandType EnumVal>
void Command<T, EnumVal>::addToBuffer(CommandBuffer* buffer) const {
    buffer->add(static_cast<const T&>(*this));
}

inline void DELETE_CMD(CommandBase*& cmd) noexcept {
    assert(cmd != nullptr);
    const Deleter& deleter = cmd->getDeleter();
    deleter.del(cmd);
    assert(cmd == nullptr);
}

inline size_t RESERVE_CMD(U8 typeIndex) noexcept {
    const CommandType cmdType = static_cast<CommandType>(typeIndex);
    switch (cmdType) {
        case CommandType::DRAW_COMMANDS: return 10;
        default: break;
    }

    return 5;
}

template<typename T>
typename std::enable_if<std::is_base_of<CommandBase, T>::value, T*>::type
CommandBuffer::allocateCommand() {
    CommandEntry& newEntry = _commandOrder.emplace_back();
    newEntry._typeIndex = static_cast<U8>(T::EType);
    newEntry._elementIndex = _commandCount[newEntry._typeIndex]++;

    return static_cast<T*>(_commands.get(newEntry));
}

template<typename T>
typename std::enable_if<std::is_base_of<CommandBase, T>::value, T*>::type
CommandBuffer::add(const T& command) {
    T* mem = allocateCommand<T>();

    if (mem != nullptr) {
        *mem = command;
    } else {
        mem = CmdAllocator<T>::allocate(command);
        _commands.insert<T>(to_base(command.Type()), mem);
    }

    _batched = false;
    return mem;
}

template<typename T>
typename std::enable_if<std::is_base_of<CommandBase, T>::value, T*>::type
CommandBuffer::add(const T&& command) {
    T* mem = allocateCommand<T>();
    if (mem != nullptr) {
        *mem = std::move(command);
    } else {
        mem = CmdAllocator<T>::allocate(std::move(command));
        _commands.insert<T>(to_base(T::EType), mem);
    }

    _batched = false;
    return mem;
}

template<typename T>
[[nodiscard]] typename std::enable_if<std::is_base_of<CommandBase, T>::value, T*>::type
CommandBuffer::get(const CommandEntry& commandEntry) noexcept {
    return static_cast<T*>(_commands.get(commandEntry));
}

template<typename T>
[[nodiscard]] typename std::enable_if<std::is_base_of<CommandBase, T>::value, T*>::type
CommandBuffer::get(const CommandEntry& commandEntry) const noexcept {
    return static_cast<T*>(_commands.get(commandEntry));
}

template<typename T>
[[nodiscard]] typename std::enable_if<std::is_base_of<CommandBase, T>::value, const CommandBuffer::Container::EntryList&>::type
CommandBuffer::get() const noexcept {
    return _commands.get(to_base(T::EType));
}

inline bool CommandBuffer::exists(const CommandEntry& commandEntry) const noexcept {
    return _commands.exists(commandEntry);
}

template<typename T>
typename std::enable_if<std::is_base_of<CommandBase, T>::value, bool>::type
CommandBuffer::exists(U24 index) const noexcept {
    return exists(to_base(T::EType), index);
}

template<typename T>
[[nodiscard]] typename std::enable_if<std::is_base_of<CommandBase, T>::value, T*>::type
CommandBuffer::get(U24 index) noexcept {
    return get<T>({to_base(T::EType), index});
}

template<typename T>
[[nodiscard]] typename std::enable_if<std::is_base_of<CommandBase, T>::value, T*>::type
CommandBuffer::get(U24 index) const noexcept {
    return get<T>({to_base(T::EType), index });
}

inline bool CommandBuffer::exists(const U8 typeIndex, const U24 index) const noexcept {
    return _commands.exists({ typeIndex, index });
}

template<typename T>
typename std::enable_if<std::is_base_of<CommandBase, T>::value, size_t>::type
CommandBuffer::count() const noexcept {
    return _commands.get(to_base(T::EType)).size();
}

inline CommandBuffer::CommandOrderContainer& CommandBuffer::operator()() noexcept {
    return _commandOrder;
}

inline const CommandBuffer::CommandOrderContainer& CommandBuffer::operator()() const noexcept {
    return _commandOrder;
}

inline void CommandBuffer::clear(const bool clearMemory) {
    _commandCount.fill(0u);
    if (clearMemory) {
        _commandOrder.clear();
        _commands.clear(true);
    } else {
        _commandOrder.resize(0);
    }

    _batched = true;
}

inline bool CommandBuffer::empty() const noexcept {
    return _commandOrder.empty();
}

template<typename T>
typename std::enable_if<std::is_base_of<CommandBase, T>::value, bool>::type
CommandBuffer::tryMergeCommands(const CommandType type, T* prevCommand, T* crtCommand) const {
    OPTICK_EVENT();

    bool ret = false;
    assert(prevCommand != nullptr && crtCommand != nullptr);
    switch (type) {
        case CommandType::DRAW_COMMANDS:        {
            ret = Merge(static_cast<DrawCommand*>(prevCommand), static_cast<DrawCommand*>(crtCommand));
        } break;
        case CommandType::BIND_DESCRIPTOR_SETS: {
            bool partial = false;
            ret = Merge(reinterpret_cast<BindDescriptorSetsCommand*>(prevCommand)->_set, reinterpret_cast<BindDescriptorSetsCommand*>(crtCommand)->_set, partial);
        } break;
        case CommandType::SEND_PUSH_CONSTANTS:  {
            bool partial = false;
            ret = Merge(reinterpret_cast<SendPushConstantsCommand*>(prevCommand)->_constants, reinterpret_cast<SendPushConstantsCommand*>(crtCommand)->_constants, partial);
        } break;
        case CommandType::DRAW_TEXT:            {
            const TextElementBatch::BatchType& crt = reinterpret_cast<DrawTextCommand*>(crtCommand)->_batch._data;
            if (!crt.empty()) {
                TextElementBatch::BatchType& prev = reinterpret_cast<DrawTextCommand*>(prevCommand)->_batch._data;
                prev.insert(std::cend(prev), std::cbegin(crt), std::cend(crt));
                ret = true;
            }
        } break;
        default: {
            ret = false;
        } break;
    }

    return ret;
}

}; //namespace GFX
}; //namespace Divide


#endif //_COMMAND_BUFFER_INL_