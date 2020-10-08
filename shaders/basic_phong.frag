#version 430 core

uniform bool lighting;
uniform sampler2D diffuse_texture;
uniform sampler2D specular_texture;
uniform sampler2D normal_texture;

uniform bool debugSwitch;

in vec2 ex_Texture_coords;
//in vec3 ex_Normal;
in vec3 ex_View;
in vec3 ex_Light;
out vec4 res_Color;
in vec3 baryColor;

// vec3 bumpNormal(vec3 normal){
// 	// create a surface angle out of bump map values
// 	// so that an angling factor of 0.f (black) means a surface normal 
// 	// pointing towards the right, and 1.f (white) means a surface normal
// 	// pointing upwards (i.e. angle of incidence for full reflection )

// 	// the angling factor around z axis for the normal
// 	float factor = texture2D(bump_texture, ex_Texture_coords.xy).r;
// 	mat3 rotationMat = mat3(
// 						1-factor, factor, 0.f, 		// col 1
// 					   -factor, 1-factor, 0.f, 		// col 2
// 						0.f, 	0.f, 	  1.f );	// col 3
// 	return rotationMat * normal;
// }

void main() {
	vec3 specColor = vec3(1.f);
	
	vec4 diffColor = texture2D(diffuse_texture, ex_Texture_coords.xy);
	
	vec3 normal = texture2D(normal_texture, ex_Texture_coords).rgb;
	//normal = bumpNormal(normal);


    if(lighting) {
		vec3 v = normalize(ex_View );
		vec3 l = normalize(ex_Light);
		vec3 n = normalize(normal);
		vec3 h = normalize(v+l);
		float diffFactor = max(0.f, dot(l, n)); // Lambert's factor
		float specFactor = 0.f;

		float shininess = 0.f;
		if(debugSwitch){
			// shows colors to debug interpolation of the UVW barycentric coords 
			// in the evaluation shader
			res_Color = vec4(baryColor, 1.f); 
			
		} 
		else {
			shininess = texture2D(specular_texture, ex_Texture_coords.xy).r * 255.f;
			if(shininess < 255.f) {
				specFactor = pow(max(0.f, dot(h, n)), shininess);
			}
		
			vec3 lightweighting =  
								+ vec3(diffColor) * diffFactor 
								+ specColor * specFactor;
			res_Color = vec4(lightweighting, diffColor.a);

		}	

	} 
	else {
		res_Color = diffColor;
	}
}