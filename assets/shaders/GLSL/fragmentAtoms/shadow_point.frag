
void applyShadowPoint(in int shadowIndex, inout float shadow) {
	// SHADOW MAPS
	vec4 position_ls = dvd_LightSource[shadowIndex]._position;
	vec4 abs_position = abs(position_ls);
	float fs_z = -max(abs_position.x, max(abs_position.y, abs_position.z));
	vec4 clip = _shadowCoord[shadowIndex] * vec4(0.0, 0.0, fs_z, 1.0);
	float depth = (clip.z / clip.w) * 0.5 + 0.5;
	shadow = texture(texDepthMapFromLightCube, vec4(position_ls.xyz, depth));
}