uniform vec3  scale;
uniform float grassScale;
uniform float time;
uniform vec2 windDirection;
uniform float windSpeed;
uniform float lod_metric;

float time2 = time * 0.001; //to seconds

void computeFoliageMovementTree(inout vec4 vertexData) {

	vec4 vertexM = gl_TextureMatrix[0] * gl_ModelViewMatrix * vec4(0.0, 0.0, 0.0, 1.0);
	float move_speed = (float(int(vertexM.y*vertexM.z) % 50)/50.0 + 0.5);
	float timeTree = time2 * windSpeed * move_speed;
	float amplituted = pow(vertexData.y, 2.0);
	vertexData.x += 0.01 * amplituted * cos(timeTree + vertexM.x) *windDirection.x;
	vertexData.z += 0.05 *scale.y* amplituted * cos(timeTree + vertexM.z) *windDirection.y; ///wd.y is actually wd.z in code
}

void computeFoliageMovementGrass(in vec3 normalData, in vec3 normalMV, inout vec4 vertexData) {
	if(normalData.y < 0.0 ) {
		normalMV = -normalMV;
		float timeGrass = time2 * windSpeed;
		float cosX = cos(vertexData.x);
		float sinX = sin(vertexData.x);
		float halfScale = 0.5*grassScale;
		vertexData.x += (halfScale*cos(timeGrass) * cosX * sinX)*windDirection.x;
		vertexData.z += (halfScale*sin(timeGrass) * cosX * sinX)*windDirection.y;
	}
}