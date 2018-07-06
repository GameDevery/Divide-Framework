#include "Headers/DoFPreRenderOperator.h"

#include "Hardware/Video/Headers/GFXDevice.h"
#include "Core/Resources/Headers/ResourceCache.h"
#include "Geometry/Shapes/Headers/Predefined/Quad3D.h"

DoFPreRenderOperator::DoFPreRenderOperator(Quad3D* target, 
										   FrameBufferObject* result, 
										   const vec2<U16>& resolution) : PreRenderOperator(DOF_STAGE,target,resolution),
																	      _outputFBO(result)
{
	_outputFBO->Create(_resolution.width, _resolution.height);
	_dofShader = CreateResource<ShaderProgram>(ResourceDescriptor("DepthOfField"));
}

DoFPreRenderOperator::~DoFPreRenderOperator() {
	RemoveResource(_dofShader);
}

void DoFPreRenderOperator::reshape(I32 width, I32 height){
	_outputFBO->Create(width, height);
}

void DoFPreRenderOperator::operation(){
	if(!_enabled || !_renderQuad) return;

	if(_inputFBO.empty() || _inputFBO.size() != 2){
		ERROR_FN(Locale::get("ERROR_DOF_INPUT_FBO"));
	}

	GFXDevice& gfx = GFX_DEVICE;

	gfx.toggle2D(true);
	_outputFBO->Begin();
		_dofShader->bind();

			_inputFBO[0]->Bind(0); ///screenFBO
			_inputFBO[1]->Bind(1); ///depthFBO
			_dofShader->UniformTexture("texScreen", 0);
			_dofShader->UniformTexture("texDepth",1);
		
			gfx.renderModel(_renderQuad);
				
			_inputFBO[1]->Unbind(1);
			_inputFBO[0]->Unbind(0);
		
	_outputFBO->End();
	
	gfx.toggle2D(false);
}