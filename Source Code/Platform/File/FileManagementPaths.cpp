#include "stdafx.h"

#include "Core/Headers/XMLEntryData.h"
#include "Core/Headers/Configuration.h"
#include "Core/Headers/PlatformContext.h"
#include "Headers/FileManagement.h"

namespace Divide {

ResourcePath Paths::g_logPath = ResourcePath("logs/");

ResourcePath Paths::g_rootPath;
ResourcePath Paths::g_assetsLocation;
ResourcePath Paths::g_shadersLocation;
ResourcePath Paths::g_texturesLocation;
ResourcePath Paths::g_proceduralTxturesLocation;
ResourcePath Paths::g_heightmapLocation;
ResourcePath Paths::g_climatesLowResLocation;
ResourcePath Paths::g_climatesMedResLocation;
ResourcePath Paths::g_climatesHighResLocation;
ResourcePath Paths::g_xmlDataLocation;
ResourcePath Paths::g_scenesLocation;

ResourcePath Paths::g_saveLocation;
ResourcePath Paths::g_imagesLocation;
ResourcePath Paths::g_materialsLocation;
ResourcePath Paths::g_navMeshesLocation;
ResourcePath Paths::g_GUILocation;
ResourcePath Paths::g_fontsPath;
ResourcePath Paths::g_soundsLocation;
ResourcePath Paths::g_localisationPath;
ResourcePath Paths::g_cacheLocation;
ResourcePath Paths::g_terrainCacheLocation;
ResourcePath Paths::g_geometryCacheLocation;
ResourcePath Paths::g_collisionMeshCacheLocation;

ResourcePath Paths::Editor::g_saveLocation;
ResourcePath Paths::Editor::g_tabLayoutFile;
ResourcePath Paths::Editor::g_panelLayoutFile;

ResourcePath Paths::Scripts::g_scriptsLocation;
ResourcePath Paths::Scripts::g_scriptsAtomsLocation;

ResourcePath Paths::Textures::g_metadataLocation;

ResourcePath Paths::Shaders::g_cacheLocation;
ResourcePath Paths::Shaders::g_cacheLocationText;
ResourcePath Paths::Shaders::g_cacheLocationBin;

// these must match the last 4 characters of the atom file
Str8 Paths::Shaders::GLSL::g_fragAtomExt;
Str8 Paths::Shaders::GLSL::g_vertAtomExt;
Str8 Paths::Shaders::GLSL::g_geomAtomExt;
Str8 Paths::Shaders::GLSL::g_tescAtomExt;
Str8 Paths::Shaders::GLSL::g_teseAtomExt;
Str8 Paths::Shaders::GLSL::g_compAtomExt;
Str8 Paths::Shaders::GLSL::g_comnAtomExt;

// Shader subfolder name that contains shader files for OpenGL
ResourcePath Paths::Shaders::GLSL::g_parentShaderLoc;
// Atom folder names in parent shader folder
ResourcePath Paths::Shaders::GLSL::g_fragAtomLoc;
ResourcePath Paths::Shaders::GLSL::g_vertAtomLoc;
ResourcePath Paths::Shaders::GLSL::g_geomAtomLoc;
ResourcePath Paths::Shaders::GLSL::g_tescAtomLoc;
ResourcePath Paths::Shaders::GLSL::g_teseAtomLoc;
ResourcePath Paths::Shaders::GLSL::g_compAtomLoc;
ResourcePath Paths::Shaders::GLSL::g_comnAtomLoc;

ResourcePath Paths::Shaders::HLSL::g_parentShaderLoc;

std::regex Paths::g_includePattern;
std::regex Paths::g_definePattern;
std::regex Paths::g_usePattern;

void Paths::initPaths(const SysInfo& info) {
    g_rootPath = ResourcePath(info._workingDirectory);
    g_logPath = ResourcePath("logs/");
    g_xmlDataLocation = ResourcePath("XML/");
    g_assetsLocation = ResourcePath("assets/");
    g_shadersLocation = ResourcePath("shaders/");
    g_texturesLocation = ResourcePath("textures/");
    g_proceduralTxturesLocation = ResourcePath("procTextures/");
    g_heightmapLocation = ResourcePath("terrain/");
    g_climatesLowResLocation = ResourcePath("climates_05k/");
    g_climatesMedResLocation = ResourcePath("climates_1k/");
    g_climatesHighResLocation = ResourcePath("climates_4k/");
    g_scenesLocation = ResourcePath("Scenes/");

    g_saveLocation = ResourcePath("SaveData/");
    g_imagesLocation = ResourcePath("misc_images/");
    g_materialsLocation = ResourcePath("materials/");
    g_navMeshesLocation = ResourcePath("navMeshes/");
    g_GUILocation = ResourcePath("GUI/");
    g_fontsPath = ResourcePath("fonts/");
    g_soundsLocation = ResourcePath("sounds/");
    g_localisationPath = ResourcePath("localisation/");
    g_cacheLocation = ResourcePath("cache/");
    g_terrainCacheLocation = ResourcePath("terrain/");
    g_geometryCacheLocation = ResourcePath("geometry/");
    g_collisionMeshCacheLocation = ResourcePath("collisionMeshes/");

    Editor::g_saveLocation = ResourcePath("Editor/");
    Editor::g_tabLayoutFile = ResourcePath("Tabs.layout");
    Editor::g_panelLayoutFile = ResourcePath("Panels.layout");

    Scripts::g_scriptsLocation = g_assetsLocation + "scripts/";
    Scripts::g_scriptsAtomsLocation = Scripts::g_scriptsLocation + "atoms/";

    Textures::g_metadataLocation = ResourcePath("textureData/");

    Shaders::g_cacheLocation = ResourcePath("shaders/");
    Shaders::g_cacheLocationText = Shaders::g_cacheLocation + "Text/";
    Shaders::g_cacheLocationBin = Shaders::g_cacheLocation + "Binary/";

    // these must match the last 4 characters of the atom file
    Shaders::GLSL::g_fragAtomExt = "frag";
    Shaders::GLSL::g_vertAtomExt = "vert";
    Shaders::GLSL::g_geomAtomExt = "geom";
    Shaders::GLSL::g_tescAtomExt = "tesc";
    Shaders::GLSL::g_teseAtomExt = "tese";
    Shaders::GLSL::g_compAtomExt = "comp";
    Shaders::GLSL::g_comnAtomExt = ".cmn";

    // Shader subfolder name that contains shader files for OpenGL
    Shaders::GLSL::g_parentShaderLoc = ResourcePath("GLSL/");
    // Atom folder names in parent shader folder
    Shaders::GLSL::g_fragAtomLoc = ResourcePath("fragmentAtoms/");
    Shaders::GLSL::g_vertAtomLoc = ResourcePath("vertexAtoms/");
    Shaders::GLSL::g_geomAtomLoc = ResourcePath("geometryAtoms/");
    Shaders::GLSL::g_tescAtomLoc = ResourcePath("tessellationCAtoms/");
    Shaders::GLSL::g_teseAtomLoc = ResourcePath("tessellationEAtoms/");
    Shaders::GLSL::g_compAtomLoc = ResourcePath("computeAtoms/");
    Shaders::GLSL::g_comnAtomLoc = ResourcePath("common/");

    Shaders::HLSL::g_parentShaderLoc = ResourcePath("HLSL/");

    g_includePattern = std::regex(R"(^\s*#\s*include\s+["<]([^">]+)*[">])");
    g_definePattern = std::regex(R"(([#!][A-z]{2,}[\s]{1,}?([A-z]{2,}[\s]{1,}?)?)([\\(]?[^\s\\)]{1,}[\\)]?)?)");
    g_usePattern = std::regex(R"(^\s*use\s*\(\s*\"(.*)\"\s*\))");
}

void Paths::updatePaths(const PlatformContext& context) {
    const Configuration& config = context.config();
    const XMLEntryData& entryData = context.entryData();
        
    g_assetsLocation = ResourcePath(entryData.assetsLocation + "/");
    g_shadersLocation = ResourcePath(config.defaultAssetLocation.shaders + "/");
    g_texturesLocation = ResourcePath(config.defaultAssetLocation.textures + "/");
    g_scenesLocation = ResourcePath(entryData.scenesLocation + "/");
    Scripts::g_scriptsLocation = g_assetsLocation + "scripts/";
    Scripts::g_scriptsAtomsLocation = Scripts::g_scriptsLocation + "atoms/";
}

}; //namespace Divide