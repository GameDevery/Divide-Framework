#include "stdafx.h"

#include "Headers/CommandParser.h"

namespace Divide {

CommandParser::CommandParser() noexcept
{
}

CommandParser::~CommandParser()
{
    _commandMap.clear();
}
};  // namespace Divide
