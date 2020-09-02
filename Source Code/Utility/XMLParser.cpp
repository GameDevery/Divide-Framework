#include "stdafx.h"

#include "Headers/XMLParser.h"

#include "Core/Headers/Configuration.h"
#include "Core/Headers/StringHelper.h"
#include "Environment/Terrain/Headers/Terrain.h"
#include "Geometry/Material/Headers/Material.h"
#include "Managers/Headers/SceneManager.h"
#include "Platform/File/Headers/FileManagement.h"
#include "Scenes/Headers/SceneInput.h"

#include <boost/property_tree/xml_parser.hpp>

namespace Divide {
namespace XML {

using boost::property_tree::ptree;

bool loadFromXML(IXMLSerializable& object, const char* file) {
    return object.fromXML(file);
}

bool saveToXML(const IXMLSerializable& object, const char* file) {
    return object.toXML(file);
}

namespace {
    ptree g_emptyPtree;
}

namespace detail {
    bool LoadSave::read(const stringImpl& path) {
        _loadPath = path;
        read_xml(_loadPath, XmlTree, boost::property_tree::xml_parser::trim_whitespace);
        return !XmlTree.empty();
    }

    bool LoadSave::prepareSaveFile(const stringImpl& path) const {
        _savePath = path;
        return createFile(_savePath.c_str(), true);
    }

    void LoadSave::write() const {
        write_xml(_savePath,
            XmlTree,
            std::locale(),
            boost::property_tree::xml_writer_make_settings<boost::property_tree::iptree::key_type>('\t', 1));
    }
};

void populatePressRelease(const ptree & attributes, PressReleaseActions::Entry& entryOut) {
    static vectorEASTL<std::string> modifiersOut, actionsUpOut, actionsDownOut;

    entryOut.clear();
    modifiersOut.resize(0);
    actionsUpOut.resize(0);
    actionsDownOut.resize(0);

    U16 id = 0;

    const std::string modifiers = attributes.get<std::string>("modifier", "");
    if (!modifiers.empty()) {
        Util::Split<vectorEASTL<std::string>, std::string>(modifiers.c_str(), ',', modifiersOut);
        for (const auto& it : modifiersOut) {
            for (U8 i = 0; i < to_base(PressReleaseActions::Modifier::COUNT); ++i) {
                if (it == PressReleaseActions::s_modifierNames[i]) {
                    entryOut.modifiers().insert(PressReleaseActions::s_modifierMappings[i]);
                    break;
                }
            }
        }
    }

    const std::string actionsUp = attributes.get<std::string>("actionUp", "");
    if (!actionsUp.empty()) {
        Util::Split<vectorEASTL<std::string>, std::string>(actionsUp.c_str(), ',', actionsUpOut);
        for (const auto& it : actionsUpOut) {
            if (!it.empty()) {
                std::stringstream ss(Util::Trim(it));
                ss >> id;
                if (!ss.fail()) {
                    entryOut.releaseIDs().insert(id);
                }
            }
        }
    }

    const std::string actionsDown = attributes.get<std::string>("actionDown", "");
    if (!actionsDown.empty()) {
        Util::Split<vectorEASTL<std::string>, std::string>(actionsDown.c_str(), ',', actionsDownOut);
        for (const auto& it : actionsDownOut) {
            if (!it.empty()) {
                std::stringstream ss(Util::Trim(it));
                ss >> id;
                if (!ss.fail()) {
                    entryOut.pressIDs().insert(id);
                }
            }
        }
    }
}

void loadDefaultKeyBindings(const stringImpl &file, Scene* scene) {
    ptree pt;
    Console::printfn(Locale::get(_ID("XML_LOAD_DEFAULT_KEY_BINDINGS")), file.c_str());
    read_xml(file, pt);

    for(const ptree::value_type & f : pt.get_child("actions", g_emptyPtree))
    {
        const ptree & attributes = f.second.get_child("<xmlattr>", g_emptyPtree);
        scene->input()->actionList().getInputAction(attributes.get<U16>("id", 0)).displayName(attributes.get<stringImpl>("name", "").c_str());
    }

    PressReleaseActions::Entry entry = {};
    for (const ptree::value_type & f : pt.get_child("keys", g_emptyPtree))
    {
        if (f.first == "<xmlcomment>") {
            continue;
        }

        
        const ptree & attributes = f.second.get_child("<xmlattr>", g_emptyPtree);
        populatePressRelease(attributes, entry);

        const Input::KeyCode key = Input::keyCodeByName(Util::Trim(f.second.data()).c_str());
        scene->input()->addKeyMapping(key, entry);
    }

    for (const ptree::value_type & f : pt.get_child("mouseButtons", g_emptyPtree))
    {
        if (f.first == "<xmlcomment>") {
            continue;
        }

        const ptree & attributes = f.second.get_child("<xmlattr>", g_emptyPtree);
        populatePressRelease(attributes, entry);

        const Input::MouseButton btn = Input::mouseButtonByName(Util::Trim(f.second.data()));

        scene->input()->addMouseMapping(btn, entry);
    }

    const stringImpl label("joystickButtons.joystick");
    for (U32 i = 0 ; i < to_base(Input::Joystick::COUNT); ++i) {
        const Input::Joystick joystick = static_cast<Input::Joystick>(i);
        
        for (const ptree::value_type & f : pt.get_child(label + std::to_string(i + 1), g_emptyPtree))
        {
            if (f.first == "<xmlcomment>") {
                continue;
            }

            const ptree & attributes = f.second.get_child("<xmlattr>", g_emptyPtree);
            populatePressRelease(attributes, entry);

            const Input::JoystickElement element = Input::joystickElementByName(Util::Trim(f.second.data()));

            scene->input()->addJoystickMapping(joystick, element._type, element._elementIndex, entry);
        }
    }
}

void loadMusicPlaylist(const Str256& scenePath, const Str64& fileName, Scene* const scene, const Configuration& config) {
    const stringImpl file = scenePath + "/" + fileName.c_str();

    if (!fileExists(file.c_str())) {
        return;
    }
    Console::printfn(Locale::get(_ID("XML_LOAD_MUSIC")), file.c_str());
    ptree pt;
    read_xml(file, pt);

    for (const ptree::value_type & f : pt.get_child("backgroundThemes", g_emptyPtree))
    {
        const ptree & attributes = f.second.get_child("<xmlattr>", g_emptyPtree);
        scene->addMusic(MusicType::TYPE_BACKGROUND,
                        attributes.get<stringImpl>("name", "").c_str(),
                        Paths::g_assetsLocation + attributes.get<stringImpl>("src", "").c_str());
    }
}

void writeXML(const stringImpl& path, const boost::property_tree::ptree& tree) {
    static boost::property_tree::xml_writer_settings<std::string> settings(' ', 4);

    boost::property_tree::write_xml(path, tree, std::locale(), settings);
}

void readXML(const stringImpl& path, boost::property_tree::ptree& tree) {
    try {
        boost::property_tree::read_xml(path, tree);
    } catch (const boost::property_tree::xml_parser_error& e) {
        Console::errorfn(Locale::get(_ID("ERROR_XML_INVALID_FILE")), path.c_str());
        stringImpl error(e.what());
        error += " [check error log!]";
        throw error.c_str();
    }
}
};  // namespace XML
};  // namespace Divide

