-- Vertex
uniform float time;
uniform vec3  scale;
uniform float windDirectionX;
uniform float windDirectionZ;
uniform float windSpeed;

uniform mat4 modelViewInvMatrix;
uniform mat4 modelViewProjectionMatrix;

#include "lightingDefaults.vert"

void main(void){
	
	vec4 position = gl_Vertex;
	
	vec4 vertexM = gl_TextureMatrix[0] * gl_ModelViewMatrix * vec4(0.0, 0.0, 0.0, 1.0);
	position.x += 0.05 * cos(time*windSpeed) * cos(position.x) * sin(position.x) *windDirectionX;
	position.z += 0.05 * sin(time*windSpeed) * cos(position.x) * sin(position.x) *windDirectionZ;

	gl_Position = gl_ModelViewProjectionMatrix * position;
	
	computeLightVectors();
}

-- Fragment.Texture

uniform bool enableFog;

#include "phong_lighting.frag"
#include "fog.frag"

void main (void){

	gl_FragDepth = gl_FragCoord.z;
	vec4 vPixToLightTBNcurrent = vPixToLightTBN[0];
	
	vec4 color = Phong(texCoord[0].st*tile_factor, vec3(0.0, 0.0, 1.0), vPixToEyeTBN, vPixToLightTBNcurrent);
	
	if(hasOpacity ){
		vec4 alpha = texture2D(opacityMap, texCoord[0].st*tile_factor);
		if(alpha.a < 0.2) discard;
	}else{
		if(color.a < 0.2) discard;	
	}
	if(enableFog){
		color = applyFog(color);
	}
	gl_FragData[0] = color;
}
	
-- Fragment.Bump

uniform bool enableFog;

#include "phong_lighting.frag"
#include "bumpMapping.frag"
#include "fog.frag"

void main (void){

	gl_FragDepth = gl_FragCoord.z;
	vec4 vPixToLightTBNcurrent = vPixToLightTBN[0];
	
	vec4 color;
	//Else, use appropriate lighting / bump mapping / relief mapping / normal mapping
	if(mode == MODE_PHONG)
		color = Phong(texCoord[0].st*tile_factor, vNormalMV, vPixToEyeTBN, vPixToLightTBNcurrent);
	else if(mode == MODE_RELIEF)
		color = ReliefMapping(texCoord[0].st*tile_factor);
	else if(mode == MODE_BUMP)
		color = NormalMapping(texCoord[0].st*tile_factor, vPixToEyeTBN, vPixToLightTBNcurrent, false);
	else if(mode == MODE_PARALLAX)
		color = NormalMapping(texCoord[0].st*tile_factor, vPixToEyeTBN, vPixToLightTBNcurrent, true);
	
	if(hasOpacity ){
		vec4 alpha = texture2D(opacityMap, texCoord[0].st*tile_factor);
		if(alpha.a < 0.2) discard;
	}else{
		if(color.a < 0.2) discard;	
	}
	if(enableFog){
		color = applyFog(color);
	}
	gl_FragData[0] = color;
}