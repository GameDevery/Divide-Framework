#include "Headers/XMLParser.h"

#include "Core/Headers/Application.h"
#include "Core/Headers/ParamHandler.h"
#include "Managers/Headers/SceneManager.h"
#include "Hardware/Video/Headers/GFXDevice.h"
#include "Geometry/Material/Headers/Material.h"
#include "Environment/Terrain/Headers/TerrainDescriptor.h"

namespace XML {
    using boost::property_tree::ptree;
    ptree pt;

    namespace {
        const char* getFilterName(TextureFilter filter)	{
            if(filter == TEXTURE_FILTER_LINEAR)
                return "TEXTURE_FILTER_LINEAR";
            else if(filter == TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST)
                return "TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST";
            else if(filter == TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
                return "TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST";
            else if(filter == TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR)
                return "TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR";
            else if(filter == TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR)
                return "TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR";

            return "TEXTURE_FILTER_NEAREST";
        }

        TextureFilter getFilter(const char* filter)	{
            if(strcmp(filter, "TEXTURE_FILTER_LINEAR") == 0 )
                return TEXTURE_FILTER_LINEAR;
            else if(strcmp(filter, "TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST") == 0 )
                return TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST;
            else if(strcmp(filter, "TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST") == 0 )
                return TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST;
            else if(strcmp(filter, "TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR") == 0 )
                return TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR;
            else if(strcmp(filter, "TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR") == 0 )
                return TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR;

            return TEXTURE_FILTER_NEAREST;
        }

        const char* getWrapModeName(TextureWrap wrapMode) {
            if(wrapMode == TEXTURE_CLAMP)
                return "TEXTURE_CLAMP";
            else if(wrapMode == TEXTURE_CLAMP_TO_EDGE)
                return "TEXTURE_CLAMP_TO_EDGE";
            else if(wrapMode == TEXTURE_CLAMP_TO_BORDER)
                return "TEXTURE_CLAMP_TO_BORDER";
            else if(wrapMode == TEXTURE_DECAL)
                return "TEXTURE_DECAL";

            return "TEXTURE_REPEAT";
        }

        TextureWrap getWrapMode(const char* wrapMode) {
            if(strcmp(wrapMode, "TEXTURE_CLAMP") == 0)
                return TEXTURE_CLAMP;
            else if(strcmp(wrapMode, "TEXTURE_CLAMP_TO_EDGE")  == 0 )
                return TEXTURE_CLAMP_TO_EDGE;
            else if(strcmp(wrapMode, "TEXTURE_CLAMP_TO_BORDER") == 0 )
                return TEXTURE_CLAMP_TO_BORDER;
            else if(strcmp(wrapMode, "TEXTURE_DECAL") == 0 )
                return TEXTURE_DECAL;

            return TEXTURE_REPEAT;
        }

        const char* getBumpMethodName(Material::BumpMethod bumpMethod)	{
            if(bumpMethod == Material::BUMP_NORMAL)
                return "BUMP_NORMAL";
            else if(bumpMethod == Material::BUMP_PARALLAX)
                return "BUMP_PARALLAX";
            else if(bumpMethod == Material::BUMP_RELIEF)
                return "BUMP_RELIEF";

            return "BUMP_NONE";
        }

        Material::BumpMethod getBumpMethod(const char* bumpMethod) {
            if(strcmp(bumpMethod, "BUMP_NORMAL") == 0 )
                return Material::BUMP_NORMAL;
            else if(strcmp(bumpMethod, "BUMP_PARALLAX") == 0 )
                return Material::BUMP_PARALLAX;
            else if(strcmp(bumpMethod, "BUMP_RELIEF") == 0 )
                return Material::BUMP_RELIEF;

            return Material::BUMP_NONE;
        }

        const char* getTextureOperationName(Material::TextureOperation textureOp) {
            if(textureOp == Material::TextureOperation_Multiply)
                return "TEX_OP_MULTIPLY";
            else if(textureOp == Material::TextureOperation_Decal)
                return "TEX_OP_DECAL";
            else if(textureOp == Material::TextureOperation_Blend)
                return "TEX_OP_BLEND";
            else if(textureOp == Material::TextureOperation_Add)
                return "TEX_OP_ADD";
            else if(textureOp == Material::TextureOperation_SmoothAdd)
                return "TEX_OP_SMOOTH_ADD";
            else if(textureOp == Material::TextureOperation_SignedAdd)
                return "TEX_OP_SIGNED_ADD";
            else if(textureOp == Material::TextureOperation_Divide)
                return "TEX_OP_DIVIDE";
            else if(textureOp == Material::TextureOperation_Subtract)
                return "TEX_OP_SUBTRACT";
            else if(textureOp == Material::TextureOperation_Combine)
                return "TEX_OP_COMBINE";

            return "TEX_OP_REPLACE";
        }

        Material::TextureOperation getTextureOperation(const char* operation) {
            if(strcmp(operation, "TEX_OP_MULTIPLY") == 0 )
                return Material::TextureOperation_Multiply;
            else if(strcmp(operation, "TEX_OP_DECAL") == 0 )
                return Material::TextureOperation_Decal;
            else if(strcmp(operation, "TEX_OP_BLEND") == 0 )
                return Material::TextureOperation_Blend;
            else if(strcmp(operation, "TEX_OP_ADD") == 0 )
                return Material::TextureOperation_Add;
            else if(strcmp(operation, "TEX_OP_SMOOTH_ADD") == 0 )
                return Material::TextureOperation_SmoothAdd;
            else if(strcmp(operation, "TEX_OP_SIGNED_ADD") == 0 )
                return Material::TextureOperation_SignedAdd;
            else if(strcmp(operation, "TEX_OP_DIVIDE") == 0 )
                return Material::TextureOperation_Divide;
            else if(strcmp(operation, "TEX_OP_SUBTRACT") == 0 )
                return Material::TextureOperation_Subtract;
            else if(strcmp(operation, "TEX_OP_COMBINE") == 0 )
                return Material::TextureOperation_Combine;

            return Material::TextureOperation_Replace;
        }

        void saveTextureXML(const std::string& textureNode, const char* operation, Texture* texture) {
            const SamplerDescriptor& sampler = texture->getCurrentSampler();
            pt.put(textureNode+".file",texture->getResourceLocation());
            pt.put(textureNode+".flip",texture->isFlipped());
            pt.put(textureNode+".MapU", getWrapModeName(sampler.wrapU()));
            pt.put(textureNode+".MapV", getWrapModeName(sampler.wrapV()));
            pt.put(textureNode+".MapW", getWrapModeName(sampler.wrapW()));
            pt.put(textureNode+".minFilter",getFilterName(sampler.minFilter()));
            pt.put(textureNode+".magFilter",getFilterName(sampler.magFilter()));
            pt.put(textureNode+".anisotrophy",(U32)sampler.anisotrophyLevel());
            pt.put(textureNode+".operation", operation);
        }

        Texture* loadTextureXML(const std::string& textureNode, const std::string& textureName) {
            std::string img_name = textureName.substr( textureName.find_last_of( '/' ) + 1 );
            std::string pathName = textureName.substr( 0, textureName.rfind("/")+1 );

            TextureWrap wrapU = getWrapMode(pt.get<std::string>(textureNode+".MapU","TEXTURE_REPEAT").c_str());
            TextureWrap wrapV = getWrapMode(pt.get<std::string>(textureNode+".MapV","TEXTURE_REPEAT").c_str());
            TextureWrap wrapW = getWrapMode(pt.get<std::string>(textureNode+".MapW","TEXTURE_REPEAT").c_str());
            TextureFilter minFilterValue = getFilter(pt.get<std::string>(textureNode+".minFilter","TEXTURE_FILTER_LINEAR").c_str());
            TextureFilter magFilterValue = getFilter(pt.get<std::string>(textureNode+".magFilter","TEXTURE_FILTER_LINEAR").c_str());
            U32 anisotrophy = pt.get(textureNode+".anisotrophy", 0);

            SamplerDescriptor sampDesc;
            sampDesc.setWrapMode(wrapU,wrapV,wrapW);
            sampDesc.setFilters(minFilterValue,magFilterValue);
            sampDesc.setAnisotrophy(anisotrophy);

            ResourceDescriptor texture(img_name);
            texture.setResourceLocation(pathName + img_name);
            texture.setFlag(pt.get(textureNode+".flip",true));
            texture.setPropertyDescriptor<SamplerDescriptor>(sampDesc);

            return CreateResource<Texture2D>(texture);
        }

        inline std::string getRendererTypeName(RendererType type){
            switch(type){
                default:
            case RENDERER_PLACEHOLDER:
                    return "Unknown Renderer Type";
            case RENDERER_FORWARD:
                return "Forward Renderer";
            case RENDERER_DEFERRED_SHADING:
                return "Deferred Shading Renderer";
            case RENDERER_DEFERRED_LIGHTING:
                return "Deferred Lighting Renderer";
        }
    }
    }

    std::string loadScripts(const std::string& file){
        ParamHandler &par = ParamHandler::getInstance();
        PRINT_FN(Locale::get("XML_LOAD_SCRIPTS"));
        read_xml(file,pt);
        std::string activeScene("MainScene");
        par.setParam("scriptLocation",pt.get("scriptLocation","XML"));
        par.setParam("assetsLocation",pt.get("assets","assets"));
        par.setParam("scenesLocation",pt.get("scenesLocation","Scenes"));
        par.setParam("serverAddress",pt.get("server","127.0.0.1"));
        loadConfig(par.getParam<std::string>("scriptLocation") + "/" + pt.get("config","config.xml"));
        read_xml(par.getParam<std::string>("scriptLocation") + "/" + pt.get("startupScene","scenes.xml"),pt);
        activeScene = pt.get("StartupScene",activeScene);
        return activeScene;
    }

    void loadConfig(const std::string& file) {
        ParamHandler &par = ParamHandler::getInstance();
        pt.clear();
        PRINT_FN(Locale::get("XML_LOAD_CONFIG"), file.c_str());
        read_xml(file,pt);
        par.setParam("locale",pt.get("language","enGB"));
        par.setParam("logFile",pt.get("debug.logFile","none"));
        par.setParam("memFile",pt.get("debug.memFile","none"));
        par.setParam("simSpeed",pt.get("runtime.simSpeed",1));
        par.setParam("appTitle",pt.get("title","DIVIDE Framework"));
        par.setParam("defaultTextureLocation",pt.get("defaultTextureLocation","textures/"));
        par.setParam("shaderLocation",pt.get("defaultShadersLocation","shaders/"));

        U8 shadowDetailLevel = pt.get<U8>("rendering.shadowDetailLevel",DETAIL_HIGH);
        U8 shadowResolutionFactor = 1;
        switch(shadowDetailLevel){
            default:
            case DETAIL_HIGH:
                shadowResolutionFactor = 1;
                break;
            case DETAIL_MEDIUM:
                shadowResolutionFactor = 2;
                break;
            case DETAIL_LOW:
                shadowResolutionFactor = 4;
                break;
        };
        U8 aaMethod = pt.get<U8>("rendering.FSAAmethod",FS_MSAA);
        U8 aaSamples = pt.get<U8>("rendering.FSAAsamples",2);
        bool postProcessing = pt.get("rendering.enablePostFX",false);
        if(aaMethod == FS_FXAA && !postProcessing) aaMethod = FS_MSAA;
        par.setParam("postProcessing.enablePostFX",postProcessing);
        par.setParam("postProcessing.enableFXAA",((aaMethod == FS_FXAA || aaMethod == FS_MSwFXAA) && aaSamples > 0));
        par.setParam("GUI.CEGUI.ExtraStates",pt.get("GUI.CEGUI.ExtraStates",false));
        par.setParam("GUI.CEGUI.SkipRendering",pt.get("GUI.CEGUI.SkipRendering",false));
        par.setParam("GUI.defaultScheme",pt.get("GUI.defaultGUIScheme","GWEN"));
        par.setParam("GUI.consoleLayout",pt.get("GUI.consoleLayoutFile","console.layout"));
        par.setParam("GUI.editorLayout",pt.get("GUI.editorLayoutFile","editor.layout"));
        par.setParam("rendering.FSAAsamples",aaSamples);
        par.setParam("rendering.FSAAmethod",aaMethod);
        par.setParam("rendering.detailLevel",pt.get<U8>("rendering.detailLevel",DETAIL_HIGH));
        par.setParam("rendering.anisotropicFilteringLevel",pt.get<U8>("rendering.anisotropicFilteringLevel",1));
        par.setParam("rendering.fogDetailLevel",pt.get<U8>("rendering.fogDetailLevel",DETAIL_HIGH));
        par.setParam("rendering.mipMapDetailLevel",pt.get<U8>("rendering.mipMapDetailLevel",DETAIL_HIGH));
        par.setParam("rendering.shadowDetailLevel",shadowDetailLevel);
        par.setParam("rendering.shadowResolutionFactor", shadowResolutionFactor);
        par.setParam("rendering.enableShadows",pt.get("rendering.enableShadows", true));
        par.setParam("rendering.enableFog", pt.get("rendering.enableFog",true));
        U16 resWidth = pt.get("runtime.resolutionWidth",1024);
        U16 resHeight = pt.get("runtime.resolutionHeight",768);
        par.setParam("runtime.overrideRefreshRate",pt.get<bool>("runtime.overrideRefreshRate",false));
        par.setParam("runtime.targetRefreshRate",pt.get<U8>("runtime.targetRefreshRate",75));
        par.setParam("runtime.zNear",(F32)pt.get("runtime.zNear",0.1f));
        par.setParam("runtime.zFar",(F32)pt.get("runtime.zFar",1200.0f));
        par.setParam("runtime.verticalFOV",(F32)pt.get("runtime.verticalFOV",60));
        par.setParam("runtime.useGLCompatProfile",pt.get("runtime.useGLCompatProfile",true));
        par.setParam("runtime.aspectRatio",1.0f * (I32)resWidth / (I32)resHeight);
        par.setParam("runtime.resolutionWidth",resWidth);
        par.setParam("runtime.resolutionHeight",resHeight);
        par.setParam("runtime.windowedMode",pt.get("rendering.windowedMode",true));
        par.setParam("runtime.allowWindowResize",pt.get("runtime.allowWindowResize",false));
        par.setParam("runtime.enableVSync",pt.get("runtime.enableVSync",false));
        par.setParam("runtime.groundPos", pt.get("runtime.groundPos",-2000.0f)); ///<Safety net for physics actors
        Application::getInstance().setResolutionHeight(resHeight);
        Application::getInstance().setResolutionWidth(resWidth);
        if(postProcessing){
            bool enable3D = pt.get("rendering.enable3D",false);
            par.setParam("postProcessing.enable3D",enable3D);
            if(enable3D){
                par.setParam("postProcessing.anaglyphOffset",pt.get("rendering.anaglyphOffset",0.16f));
            }
            par.setParam("postProcessing.enableNoise",pt.get("rendering.enableNoise",false));
            par.setParam("postProcessing.enableDepthOfField",pt.get("rendering.enableDepthOfField",false));
            par.setParam("postProcessing.enableBloom",pt.get("rendering.enableBloom",false));
            par.setParam("postProcessing.enableSSAO",pt.get("rendering.enableSSAO",false));
            par.setParam("postProcessing.bloomFactor",pt.get("rendering.bloomFactor",0.4f));
            par.setParam("postProcessing.enableHDR",pt.get("rendering.enableHDR",false));
        }
        par.setParam("mesh.playAnimations",pt.get("mesh.playAnimations",true));

        //global fog values
        par.setParam("rendering.sceneState.fogStart",   pt.get("rendering.fogStartDistance",300.0f));
        par.setParam("rendering.sceneState.fogEnd",     pt.get("rendering.fogEndDistance",800.0f));
        par.setParam("rendering.sceneState.fogDensity", pt.get("rendering.fogDensity",0.01f));
        par.setParam("rendering.sceneState.fogColor.r", pt.get<F32>("rendering.fogColor.<xmlattr>.r", 0.2f));
        par.setParam("rendering.sceneState.fogColor.g", pt.get<F32>("rendering.fogColor.<xmlattr>.g", 0.2f));
        par.setParam("rendering.sceneState.fogColor.b",	pt.get<F32>("rendering.fogColor.<xmlattr>.b", 0.2f));
    }

    void loadScene(const std::string& sceneName, SceneManager& sceneMgr) {
        ParamHandler &par = ParamHandler::getInstance();
        pt.clear();
        PRINT_FN(Locale::get("XML_LOAD_SCENE"), sceneName.c_str());
        std::string sceneLocation = par.getParam<std::string>("scriptLocation") + "/" +
                                    par.getParam<std::string>("scenesLocation") + "/" + sceneName;
        try{
            read_xml(sceneLocation + ".xml", pt);
        }catch(	boost::property_tree::xml_parser_error & e ){
            ERROR_FN(Locale::get("ERROR_XML_INVALID_SCENE"),sceneName.c_str());
            std::string error = e.what();
            error += " [check error log!]";
            throw error.c_str();
        }
        par.setParam("currentScene",sceneName);
        Scene* scene = sceneMgr.createScene(sceneName);

        if(!scene)	{
            ERROR_FN(Locale::get("ERROR_XML_LOAD_INVALID_SCENE"));
            return;
        }

        sceneMgr.setActiveScene(scene);
        scene->setName(sceneName);
        scene->state().getGrassVisibility() = pt.get("vegetation.grassVisibility",1000.0f);
        scene->state().getTreeVisibility()  = pt.get("vegetation.treeVisibility",1000.0f);
        scene->state().getGeneralVisibility()  = pt.get("options.visibility",1000.0f);

        scene->state().getWindDirX()  = pt.get("wind.windDirX",1.0f);
        scene->state().getWindDirZ()  = pt.get("wind.windDirZ",1.0f);
        scene->state().getWindSpeed() = pt.get("wind.windSpeed",1.0f);

        scene->state().getWaterLevel() = pt.get("water.waterLevel",0.0f);
        scene->state().getWaterDepth() = pt.get("water.waterDepth",-75);
        if(boost::optional<ptree &> cameraPositionOverride = pt.get_child_optional("options.cameraStartPosition")){
            par.setParam("options.cameraStartPosition.x", pt.get("options.cameraStartPosition.<xmlattr>.x",0.0f));
            par.setParam("options.cameraStartPosition.y", pt.get("options.cameraStartPosition.<xmlattr>.y",0.0f));
            par.setParam("options.cameraStartPosition.z", pt.get("options.cameraStartPosition.<xmlattr>.z",0.0f));
            par.setParam("options.cameraStartOrientation.xOffsetDegrees",pt.get("options.cameraStartPosition.<xmlattr>.xOffsetDegrees",0.0f));
            par.setParam("options.cameraStartOrientation.yOffsetDegrees",pt.get("options.cameraStartPosition.<xmlattr>.yOffsetDegrees",0.0f));
            par.setParam("options.cameraStartPositionOverride",true);
        }else{
            par.setParam("options.cameraStartPositionOverride",false);
        }
        if(boost::optional<ptree &> physicsCook = pt.get_child_optional("options.autoCookPhysicsAssets")){
            par.setParam("options.autoCookPhysicsAssets", pt.get<bool>("options.autoCookPhysicsAssets",false));
        }else{
            par.setParam("options.autoCookPhysicsAssets", false);
        }
        if(boost::optional<ptree &> cameraPositionOverride = pt.get_child_optional("options.cameraSpeed")){
            par.setParam("options.cameraSpeed.move",pt.get("options.cameraSpeed.<xmlattr>.move",1.0f));
            par.setParam("options.cameraSpeed.turn",pt.get("options.cameraSpeed.<xmlattr>.turn",1.0f));
        }else{
            par.setParam("options.cameraSpeed.move",0.5f);
            par.setParam("options.cameraSpeed.turn",1.0f);
        }

        if(boost::optional<ptree &> fog = pt.get_child_optional("fog")){
            par.setParam("rendering.sceneState.fogStart",   pt.get("fog.fogStartDistance",300.0f));
            par.setParam("rendering.sceneState.fogEnd",     pt.get("fog.fogEndDistance",800.0f));
            par.setParam("rendering.sceneState.fogDensity", pt.get("fog.fogDensity",0.01f));
            par.setParam("rendering.sceneState.fogColor.r", pt.get<F32>("fog.fogColor.<xmlattr>.r", 0.2f));
            par.setParam("rendering.sceneState.fogColor.g", pt.get<F32>("fog.fogColor.<xmlattr>.g", 0.2f));
            par.setParam("rendering.sceneState.fogColor.b",	pt.get<F32>("fog.fogColor.<xmlattr>.b", 0.2f));
        }

        scene->state().getFogDesc()._fogStartDist = par.getParam<F32>("rendering.sceneState.fogStart");
        scene->state().getFogDesc()._fogEndDist   = par.getParam<F32>("rendering.sceneState.fogEnd");
        scene->state().getFogDesc()._fogDensity   = par.getParam<F32>("rendering.sceneState.fogDensity");
        scene->state().getFogDesc()._fogColor.set(par.getParam<F32>("rendering.sceneState.fogColor.r"),
                                                   par.getParam<F32>("rendering.sceneState.fogColor.g"),
                                                   par.getParam<F32>("rendering.sceneState.fogColor.b"));

        loadTerrain(sceneLocation + "/" + pt.get("terrain","terrain.xml"),scene);
        loadGeometry(sceneLocation + "/" + pt.get("assets","assets.xml"),scene);
    }

    void loadTerrain(const std::string &file, Scene* const scene) {
        U8 count = 0;
        pt.clear();
        PRINT_FN(Locale::get("XML_LOAD_TERRAIN"),file.c_str());
        read_xml(file,pt);
        ptree::iterator it;
        std::string assetLocation = ParamHandler::getInstance().getParam<std::string>("assetsLocation") + "/";
        for (it = pt.get_child("terrainList").begin(); it != pt.get_child("terrainList").end(); ++it )	{
            std::string name = it->second.data(); //The actual terrain name
            std::string tag = it->first.data();   //The <name> tag for valid terrains or <xmlcomment> for comments
            //Check and skip commented terrain
            if(tag.find("<xmlcomment>") != std::string::npos) continue;
            //Load the rest of the terrain
            TerrainDescriptor* ter = CreateResource<TerrainDescriptor>(name+"_descriptor");
            ter->addVariable("terrainName",name);
            ter->addVariable("heightmap",assetLocation + pt.get<std::string>(name + ".heightmap"));
            ter->addVariable("textureMap",assetLocation + pt.get<std::string>(name + ".textures.map"));
            ter->addVariable("redTexture",assetLocation + pt.get<std::string>(name + ".textures.red"));
            ter->addVariable("greenTexture",assetLocation + pt.get<std::string>(name + ".textures.green"));
            ter->addVariable("blueTexture",assetLocation + pt.get<std::string>(name + ".textures.blue"));
            ter->addVariable("alphaTexture",assetLocation + pt.get<std::string>(name + ".textures.alpha","none"));
            ter->addVariable("normalMap",assetLocation + pt.get<std::string>(name + ".textures.normalMap"));
            ter->addVariable("waterCaustics",assetLocation + pt.get<std::string>(name + ".textures.waterCaustics"));
            ter->addVariable("grassMap",assetLocation + pt.get<std::string>(name + ".vegetation.map"));
            ter->addVariable("grassBillboard1",assetLocation + pt.get<std::string>(name + ".vegetation.grassBillboard1"));
            ter->addVariable("grassBillboard2",assetLocation + pt.get<std::string>(name + ".vegetation.grassBillboard2"));
            ter->addVariable("grassBillboard3",assetLocation + pt.get<std::string>(name + ".vegetation.grassBillboard3"));
            ter->setGrassDensity(pt.get<U32>(name + ".vegetation.<xmlattr>.grassDensity"));
            ter->setTreeDensity(pt.get<U16>(name + ".vegetation.<xmlattr>.treeDensity"));
            ter->setGrassScale(pt.get<F32>(name + ".vegetation.<xmlattr>.grassScale"));
            ter->setTreeScale(pt.get<F32>(name + ".vegetation.<xmlattr>.treeScale"));
            ter->setDiffuseScale(pt.get<F32>(name+".textures.diffuseUVScale",250.0f));
            ter->setNormalMapScale(pt.get<F32>(name+".textures.normalUVScale",1000.0f));
            ter->setPosition(vec3<F32>(pt.get<F32>(name + ".position.<xmlattr>.x"),
                                       pt.get<F32>(name + ".position.<xmlattr>.y"),
                                       pt.get<F32>(name + ".position.<xmlattr>.z")));
            ter->setScale(vec2<F32>(pt.get<F32>(name + ".scale"), //width / length
                                    pt.get<F32>(name + ".heightFactor"))); //height

            ter->setActive(pt.get<bool>(name + ".active"));
            ter->setChunkSize(pt.get<U32>(name + ".nodeChunkSize"));
            if(boost::optional<ptree &> physics = pt.get_child_optional(name + ".addToPhysics")){
                ter->setCreatePXActor(pt.get<bool>(name + ".addToPhysics", false));
            }
            scene->addTerrain(ter);
            count++;
        }
        PRINT_FN(Locale::get("XML_TERRAIN_COUNT"),count);
    }

    void loadGeometry(const std::string &file, Scene* const scene) 	{
        pt.clear();
        PRINT_FN(Locale::get("XML_LOAD_GEOMETRY"),file.c_str());
        read_xml(file,pt);
        ptree::iterator it;
        std::string assetLocation = ParamHandler::getInstance().getParam<std::string>("assetsLocation")+"/";

        if(boost::optional<ptree &> geometry = pt.get_child_optional("geometry"))
        for (it = pt.get_child("geometry").begin(); it != pt.get_child("geometry").end(); ++it ) 	{
            std::string name = it->second.data();
            std::string format = it->first.data();
            if(format.find("<xmlcomment>") != std::string::npos) continue;
            FileData model;
            model.ItemName = name;
            model.ModelName  = assetLocation + pt.get<std::string>(name + ".model");
            model.position.x = pt.get<F32>(name + ".position.<xmlattr>.x",1);
            model.position.y = pt.get<F32>(name + ".position.<xmlattr>.y",1);
            model.position.z = pt.get<F32>(name + ".position.<xmlattr>.z",1);
            model.orientation.x = pt.get<F32>(name + ".orientation.<xmlattr>.x");
            model.orientation.y = pt.get<F32>(name + ".orientation.<xmlattr>.y");
            model.orientation.z = pt.get<F32>(name + ".orientation.<xmlattr>.z");
            model.scale.x    = pt.get<F32>(name + ".scale.<xmlattr>.x");
            model.scale.y    = pt.get<F32>(name + ".scale.<xmlattr>.y");
            model.scale.z    = pt.get<F32>(name + ".scale.<xmlattr>.z");
            model.type = GEOMETRY;
            model.version = pt.get<F32>(name + ".version");
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".staticObject")){
                model.staticUsage = pt.get<bool>(name + ".staticObject",false);
            }else{
                model.staticUsage = false;
            }
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".addToNavigation")){
                model.navigationUsage = pt.get<bool>(name + ".addToNavigation",false);
            }else{
                model.navigationUsage = false;
            }
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".useHighNavigationDetail")){
                model.useHighDetailNavMesh = pt.get<bool>(name + ".useHighNavigationDetail",false);
            }else{
                model.useHighDetailNavMesh = false;
            }
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".addToPhysics")){
                model.physicsUsage = pt.get<bool>(name + ".addToPhysics",false);
                model.physicsPushable = pt.get<bool>(name + ".addToPhysicsGroupPushable",false);
            }else{
                model.physicsUsage = false;
            }

            scene->addModel(model);
        }

        if(boost::optional<ptree &> vegetation = pt.get_child_optional("vegetation"))
        for (it = pt.get_child("vegetation").begin(); it != pt.get_child("vegetation").end(); ++it ) {
            std::string name = it->second.data();
            std::string format = it->first.data();
            if(format.find("<xmlcomment>") != std::string::npos) continue;
            FileData model;
            model.ItemName = name;
            model.ModelName  = assetLocation + pt.get<std::string>(name + ".model");
            model.position.x = pt.get<F32>(name + ".position.<xmlattr>.x");
            model.position.y = pt.get<F32>(name + ".position.<xmlattr>.y");
            model.position.z = pt.get<F32>(name + ".position.<xmlattr>.z");
            model.orientation.x = pt.get<F32>(name + ".orientation.<xmlattr>.x");
            model.orientation.y = pt.get<F32>(name + ".orientation.<xmlattr>.y");
            model.orientation.z = pt.get<F32>(name + ".orientation.<xmlattr>.z");
            model.scale.x    = pt.get<F32>(name + ".scale.<xmlattr>.x");
            model.scale.y    = pt.get<F32>(name + ".scale.<xmlattr>.y");
            model.scale.z    = pt.get<F32>(name + ".scale.<xmlattr>.z");
            model.type = VEGETATION;
            model.version = pt.get<F32>(name + ".version");
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".staticObject")){
                model.staticUsage = pt.get<bool>(name + ".staticObject",false);
            }else{
                model.staticUsage = false;
            }
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".addToNavigation")){
                model.navigationUsage = pt.get<bool>(name + ".addToNavigation",false);
            }else{
                model.navigationUsage = false;
            }
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".useHighNavigationDetail")){
                model.useHighDetailNavMesh = pt.get<bool>(name + ".useHighNavigationDetail",false);
            }else{
                model.useHighDetailNavMesh = false;
            }
             if(boost::optional<ptree &> child = pt.get_child_optional(name + ".addToPhysics")){
                model.physicsUsage = pt.get<bool>(name + ".addToPhysics",false);
            }else{
                model.physicsUsage = false;
            }
            scene->addModel(model);
        }

        if(boost::optional<ptree &> primitives = pt.get_child_optional("primitives"))
        for (it = pt.get_child("primitives").begin(); it != pt.get_child("primitives").end(); ++it ) {
            std::string name = it->second.data();
            std::string format = it->first.data();
            if(format.find("<xmlcomment>") != std::string::npos) continue;

            FileData model;
            model.ItemName = name;
            model.ModelName = pt.get<std::string>(name + ".model");
            model.position.x = pt.get<F32>(name + ".position.<xmlattr>.x");
            model.position.y = pt.get<F32>(name + ".position.<xmlattr>.y");
            model.position.z = pt.get<F32>(name + ".position.<xmlattr>.z");
            model.orientation.x = pt.get<F32>(name + ".orientation.<xmlattr>.x");
            model.orientation.y = pt.get<F32>(name + ".orientation.<xmlattr>.y");
            model.orientation.z = pt.get<F32>(name + ".orientation.<xmlattr>.z");
            model.scale.x    = pt.get<F32>(name + ".scale.<xmlattr>.x");
            model.scale.y    = pt.get<F32>(name + ".scale.<xmlattr>.y");
            model.scale.z    = pt.get<F32>(name + ".scale.<xmlattr>.z");
            model.color.r    = pt.get<F32>(name + ".color.<xmlattr>.r");
            model.color.g    = pt.get<F32>(name + ".color.<xmlattr>.g");
            model.color.b    = pt.get<F32>(name + ".color.<xmlattr>.b");
            /*The data variable stores a float variable (not void*) that can represent anything you want*/
            /*For Text3D, it's the line width and for Box3D it's the edge length*/
            if(model.ModelName.compare("Text3D") == 0){
                model.data = pt.get<F32>(name + ".lineWidth");
                model.data2 = pt.get<std::string>(name + ".text");
                model.data3 = pt.get<std::string>(name + ".fontName");
            }else if(model.ModelName.compare("Box3D") == 0)
                model.data = pt.get<F32>(name + ".size");
            else if(model.ModelName.compare("Sphere3D") == 0)
                model.data = pt.get<F32>(name + ".radius");
            else model.data = 0;

            model.type = PRIMITIVE;
            model.version = pt.get<F32>(name + ".version");
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".staticObject")){
                model.staticUsage = pt.get<bool>(name + ".staticObject",false);
            }else{
                model.staticUsage = false;
            }
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".addToNavigation")){
                model.navigationUsage = pt.get<bool>(name + ".addToNavigation",false);
            }else{
                model.navigationUsage = false;
            }
            if(boost::optional<ptree &> child = pt.get_child_optional(name + ".useHighNavigationDetail")){
                model.useHighDetailNavMesh = pt.get<bool>(name + ".useHighNavigationDetail",false);
            }else{
                model.useHighDetailNavMesh = false;
            }
            scene->addModel(model);
        }
    }

    Material* loadMaterial(const std::string &file){
        ParamHandler &par = ParamHandler::getInstance();
        std::string location = par.getParam<std::string>("scriptLocation") + "/" +
                                    par.getParam<std::string>("scenesLocation") + "/" +
                                    par.getParam<std::string>("currentScene") + "/materials/";

        return loadMaterialXML(location+file);
    }

    Material* loadMaterialXML(const std::string &matName){
        std::string materialFile(matName+"-"+getRendererTypeName(GFX_DEVICE.getRenderer()->getType())+".xml");
        pt.clear();
        std::ifstream inp;
        inp.open(materialFile.c_str(),
                 std::ifstream::in);
        if(inp.fail()){
            inp.clear(std::ios::failbit);
            inp.close();
            return NULL;
        }
        inp.close();
        bool skip = false;
        PRINT_FN(Locale::get("XML_LOAD_MATERIAL"),matName.c_str());
        read_xml(materialFile, pt);

        std::string materialName = matName.substr(matName.rfind("/")+1,matName.length());

        if(FindResource<Material>(materialName)) skip = true;
        Material* mat = CreateResource<Material>(ResourceDescriptor(materialName));
        if(skip) return mat;
        ///Skip if the material was cooked by a different renderer

        mat->setDiffuse(vec4<F32>(pt.get<F32>("material.diffuse.<xmlattr>.r",0.6f),
                                  pt.get<F32>("material.diffuse.<xmlattr>.g",0.6f),
                                  pt.get<F32>("material.diffuse.<xmlattr>.b",0.6f),
                                  pt.get<F32>("material.diffuse.<xmlattr>.a",1.f)));
        mat->setAmbient(vec4<F32>(pt.get<F32>("material.ambient.<xmlattr>.r",0.6f),
                                  pt.get<F32>("material.ambient.<xmlattr>.g",0.6f),
                                  pt.get<F32>("material.ambient.<xmlattr>.b",0.6f),
                                  pt.get<F32>("material.ambient.<xmlattr>.a",1.f)));
        mat->setSpecular(vec4<F32>(pt.get<F32>("material.specular.<xmlattr>.r",1.f),
                                   pt.get<F32>("material.specular.<xmlattr>.g",1.f),
                                   pt.get<F32>("material.specular.<xmlattr>.b",1.f),
                                   pt.get<F32>("material.specular.<xmlattr>.a",1.f)));
        mat->setEmissive(vec3<F32>(pt.get<F32>("material.emissive.<xmlattr>.r",1.f),
                                   pt.get<F32>("material.emissive.<xmlattr>.g",1.f),
                                   pt.get<F32>("material.emissive.<xmlattr>.b",1.f)));

        mat->setShininess(pt.get<F32>("material.shininess.<xmlattr>.v",50.f));

        mat->setDoubleSided(pt.get<bool>("material.doubleSided",false));

        Texture* tempTexture = NULL;
        if(boost::optional<ptree &> child = pt.get_child_optional("diffuseTexture1")){
            tempTexture = loadTextureXML("diffuseTexture1", pt.get("diffuseTexture1.file","none"));
            mat->setTexture(Material::TEXTURE_UNIT0, tempTexture,
                            getTextureOperation(pt.get<std::string>("diffuseTexture1.operation","TEX_OP_MULTIPLY").c_str()));
        }
        if(boost::optional<ptree &> child = pt.get_child_optional("diffuseTexture2")){
            mat->setTexture(Material::TEXTURE_UNIT0 + 1,loadTextureXML("diffuseTexture2", pt.get("diffuseTexture2.file","none")),
                            getTextureOperation(pt.get<std::string>("diffuseTexture2.operation","TEX_OP_MULTIPLY").c_str()));
        }
        if(boost::optional<ptree &> child = pt.get_child_optional("bumpMap")){
            mat->setTexture(Material::TEXTURE_NORMALMAP,loadTextureXML("bumpMap", pt.get("bumpMap.file","none")),
                            getTextureOperation(pt.get<std::string>("bumpMap.operation", "BUMP_NONE").c_str()));
            if(boost::optional<ptree &> child = pt.get_child_optional("bumpMap.method")){
                mat->setBumpMethod(getBumpMethod(pt.get<std::string>("bumpMap.method","BUMP_NORMAL").c_str()));
            }
        }
        if(boost::optional<ptree &> child = pt.get_child_optional("opacityMap")){
            mat->setTexture(Material::TEXTURE_OPACITY,loadTextureXML("opacityMap", pt.get("opacityMap.file","none")),
                            getTextureOperation(pt.get<std::string>("opacityMap.operation","TEX_OP_REPLACE").c_str()));
        }
        if(boost::optional<ptree &> child = pt.get_child_optional("specularMap")){
            mat->setTexture(Material::TEXTURE_SPECULAR,loadTextureXML("specularMap", pt.get("specularMap.file","none")),
                            getTextureOperation(pt.get<std::string>("specularMap.operation","TEX_OP_REPLACE").c_str()));
        }
        //if(boost::optional<ptree &> child = pt.get_child_optional("shaderProgram")){
        //	mat->setShaderProgram(pt.get("shaderProgram.effect","NULL_SHADER"));
        //}
        if(boost::optional<ptree &> child = pt.get_child_optional("shadows")){
            mat->setCastsShadows(pt.get<bool>("shadows.castsShadows", true));
            mat->setReceivesShadows(pt.get<bool>("shadows.receiveShadows", true));
        }
        return mat;
    }

    void dumpMaterial(Material& mat){
        if(!mat.isDirty()) return;

        ParamHandler &par = ParamHandler::getInstance();
        std::string file = mat.getName();
        file = file.substr(file.rfind("/")+1,file.length());

        std::string location = par.getParam<std::string>("scriptLocation") + "/" +
                               par.getParam<std::string>("scenesLocation") + "/" +
                               par.getParam<std::string>("currentScene") + "/materials/";

        std::string fileLocation = location +  file + "-" + getRendererTypeName(GFX_DEVICE.getRenderer()->getType()) + ".xml";
        pt.clear();
        pt.put("material.name",file);
        pt.put("material.ambient.<xmlattr>.r",mat.getMaterialMatrix().getCol(0).x);
        pt.put("material.ambient.<xmlattr>.g",mat.getMaterialMatrix().getCol(0).y);
        pt.put("material.ambient.<xmlattr>.b",mat.getMaterialMatrix().getCol(0).z);
        pt.put("material.ambient.<xmlattr>.a",mat.getMaterialMatrix().getCol(0).w);
        pt.put("material.diffuse.<xmlattr>.r",mat.getMaterialMatrix().getCol(1).x);
        pt.put("material.diffuse.<xmlattr>.g",mat.getMaterialMatrix().getCol(1).y);
        pt.put("material.diffuse.<xmlattr>.b",mat.getMaterialMatrix().getCol(1).z);
        pt.put("material.diffuse.<xmlattr>.a",mat.getMaterialMatrix().getCol(1).w);
        pt.put("material.specular.<xmlattr>.r",mat.getMaterialMatrix().getCol(2).x);
        pt.put("material.specular.<xmlattr>.g",mat.getMaterialMatrix().getCol(2).y);
        pt.put("material.specular.<xmlattr>.b",mat.getMaterialMatrix().getCol(2).z);
        pt.put("material.specular.<xmlattr>.a",mat.getMaterialMatrix().getCol(2).w);
        pt.put("material.shininess.<xmlattr>.v",mat.getMaterialMatrix().getCol(3).x);
        pt.put("material.emissive.<xmlattr>.r", mat.getMaterialMatrix().getCol(3).y);
        pt.put("material.emissive.<xmlattr>.g", mat.getMaterialMatrix().getCol(3).z);
        pt.put("material.emissive.<xmlattr>.b", mat.getMaterialMatrix().getCol(3).w);
        pt.put("material.doubleSided", mat.isDoubleSided());

        Texture* texture = NULL;

        if((texture = mat.getTexture(Material::TEXTURE_UNIT0)) != NULL){
            saveTextureXML("diffuseTexture1",getTextureOperationName(mat.getTextureOperation(Material::TEXTURE_UNIT0)), texture);
        }

        if((texture = mat.getTexture(Material::TEXTURE_UNIT0 + 1)) != NULL){
            saveTextureXML("diffuseTexture2",getTextureOperationName(mat.getTextureOperation(Material::TEXTURE_UNIT0 + 1)), texture);
        }

        if((texture = mat.getTexture(Material::TEXTURE_NORMALMAP)) != NULL){
            saveTextureXML("bumpMap", getTextureOperationName(mat.getTextureOperation(Material::TEXTURE_NORMALMAP)), texture);
            pt.put("bumpMap.method", getBumpMethodName(mat.getBumpMethod()));
        }

        if((texture = mat.getTexture(Material::TEXTURE_OPACITY)) != NULL){
            saveTextureXML("opacityMap",getTextureOperationName(mat.getTextureOperation(Material::TEXTURE_OPACITY)), texture);
        }

        if((texture = mat.getTexture(Material::TEXTURE_SPECULAR)) != NULL){
            saveTextureXML("specularMap", getTextureOperationName(mat.getTextureOperation(Material::TEXTURE_SPECULAR)), texture);
        }

        ShaderProgram* s = mat.getShaderProgram();
        if(s){
            pt.put("shaderProgram.effect",s->getName());
        }
        s = mat.getShaderProgram(DEPTH_STAGE);
        if(s){
            pt.put("shaderProgram.depthEffect",s->getName());
        }
        pt.put("shadows.castsShadows", mat.getCastsShadows());
        pt.put("shadows.receiveShadows", mat.getReceivesShadows());
        boost::property_tree::xml_writer_settings<char> settings('\t', 1);
        FILE * xml = fopen(fileLocation.c_str(), "w");
        write_xml(fileLocation, pt,std::locale(),settings);
        fclose(xml);
    }
}