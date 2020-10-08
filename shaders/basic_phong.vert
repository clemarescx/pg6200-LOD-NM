#version 430 core

uniform mat4 model_view_mat;
uniform mat4 model_mat;
uniform mat3 normal_mat;
uniform vec3 light_position;


uniform mat3 model_view_mat_3x3;
in  vec3 tangent;
in  vec3 binormal;


in  vec3 position;
in  vec3 normal;
in	vec2 UV;

out vec2 tc_Texture_coords;
out vec3 tc_Normal;
out vec3 tc_View;
out vec3 tc_Light;
out vec3 tc_Position;


void main() {
	tc_Normal = normalize((model_mat * vec4(normal, 0.f)).xyz);
	tc_Position = (model_view_mat * vec4(position, 1.0)).xyz;

	vec4 position_cameraSpace = model_view_mat * vec4(position, 1.0);
	tc_View = -position_cameraSpace.xyz;
	tc_Light = light_position - position_cameraSpace.xyz;
	vec3 light_normal =  normalize(normal_mat * normal);

	tc_Texture_coords = UV;
	
	// calculate the tangent space basis
	vec3 vertexTangent_cameraspace 		= 	model_view_mat_3x3 * tangent;
	vec3 vertexBinormal_cameraspace 	= 	model_view_mat_3x3 * binormal;
	vec3 vertexNormal_cameraspace 		= 	model_view_mat_3x3 * light_normal;

	mat3 TBN = transpose(mat3( 
		vertexTangent_cameraspace,
		vertexBinormal_cameraspace,
		vertexNormal_cameraspace 	
	));

	tc_View = TBN * tc_View;
	tc_Light = TBN * tc_Light;
}