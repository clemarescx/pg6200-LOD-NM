#include "Model.h"

#include "GameException.h"

#include <iostream>
#include <glm/gtc/matrix_transform.hpp>
#include <IL/il.h>
#include <IL/ilu.h>
#include "GLUtils/GLUtils.hpp"

Model::Model(std::string filename, bool invert){
	std::vector<float> vertex_data, normal_data, color_data, uv_data, tangent_data, binormal_data;
	aiMatrix4x4 trafo;
	aiIdentityMatrix4(&trafo);

	scene = aiImportFile(filename.c_str(), aiProcessPreset_TargetRealtime_Quality);// | aiProcess_FlipWindingOrder);
	if(!scene){
		std::string log = "Unable to load mesh from ";
		log.append(filename);
		THROW_EXCEPTION(log);
	}

	//Load the model recursively into data
	min_dim = glm::vec3(std::numeric_limits<float>::max());
	max_dim = glm::vec3(std::numeric_limits<float>::min());
	findBBoxRecursive(scene, scene->mRootNode, min_dim, max_dim, &trafo);

	loadRecursive(root, invert, vertex_data, normal_data, color_data, uv_data, tangent_data, binormal_data, scene,
	              scene->mRootNode);

	//Translate to center
	glm::vec3 translation = (max_dim - min_dim) / glm::vec3(2.0f) + min_dim;
	glm::vec3 scale_helper = glm::vec3(1.0f) / (max_dim - min_dim);
	glm::vec3 scale = glm::vec3(std::min(scale_helper.x, std::min(scale_helper.y, scale_helper.z)));
	if(invert) scale = -scale;

	root.transform = glm::scale(root.transform, scale);
	root.transform = translate(root.transform, -translation);

	n_vertices = vertex_data.size();

	//Create the VBOs from the data.
	if(fmod(static_cast<float>(n_vertices), 3.0f) < 0.000001f)
		vertices.reset(new GLUtils::VBO<GL_ARRAY_BUFFER>(vertex_data.data(), n_vertices * sizeof(float)));
	else
		THROW_EXCEPTION("The number of vertices in the mesh is wrong");
	if(normal_data.size() == n_vertices)
		normals.reset(new GLUtils::VBO<GL_ARRAY_BUFFER>(normal_data.data(), n_vertices * sizeof(float)));

	if(color_data.size() == 4 * n_vertices / 3)
		colors.reset(new GLUtils::VBO<GL_ARRAY_BUFFER>(color_data.data(), n_vertices * sizeof(float)));

	if(uv_data.size() == 2 * n_vertices / 3)
		uvs.reset(new GLUtils::VBO<GL_ARRAY_BUFFER>(uv_data.data(), n_vertices * sizeof(float)));

	if(tangent_data.size() == n_vertices)
		tangents.reset(new GLUtils::VBO<GL_ARRAY_BUFFER>(tangent_data.data(), n_vertices * sizeof(float)));
	
	if(binormal_data.size() == n_vertices)
		binormals.reset(new GLUtils::VBO<GL_ARRAY_BUFFER>(binormal_data.data(), n_vertices * sizeof(float)));

	std::cout << "Loading diffuse map... ";
	diffuse_texture = loadTexture("textures/basketball/bball_diffuse.png");
	std::cout << "Done\nLoading normal map... ";
	bump_texture = loadTexture("textures/basketball/bball_normal.png");
	std::cout << "Done\nLoading specular map... ";
	specular_texture = loadTexture("textures/basketball/bball_specular.png");
	std::cout << "Done" << std::endl;

}

Model::~Model(){ }

void Model::findBBoxRecursive(const aiScene *scene, const aiNode *node,
                              glm::vec3 &min_dim, glm::vec3 &max_dim, aiMatrix4x4 *trafo){
	aiMatrix4x4 prev;

	prev = *trafo;
	aiMultiplyMatrix4(trafo, &node->mTransformation);

	for(unsigned int n = 0; n < node->mNumMeshes; ++n){
		const struct aiMesh *mesh = scene->mMeshes[node->mMeshes[n]];
		for(unsigned int t = 0; t < mesh->mNumVertices; ++t){

			aiVector3D tmp = mesh->mVertices[t];
			aiTransformVecByMatrix4(&tmp, trafo);

			min_dim.x = std::min(min_dim.x, tmp.x);
			min_dim.y = std::min(min_dim.y, tmp.y);
			min_dim.z = std::min(min_dim.z, tmp.z);

			max_dim.x = std::max(max_dim.x, tmp.x);
			max_dim.y = std::max(max_dim.y, tmp.y);
			max_dim.z = std::max(max_dim.z, tmp.z);
		}
	}

	for(unsigned int n = 0; n < node->mNumChildren; ++n)
		findBBoxRecursive(scene, node->mChildren[n], min_dim, max_dim, trafo);
	*trafo = prev;
}

void Model::loadRecursive(MeshPart &part, bool invert,
                          std::vector<float> &vertex_data, std::vector<float> &normal_data,
                          std::vector<float> &color_data, std::vector<float> &uv_data,
                          std::vector<float> &tangent_data, std::vector<float> &binormal_data,
                          const aiScene *scene, const aiNode *node){
	//update transform matrix. notice that we also transpose it
	aiMatrix4x4 m = node->mTransformation;
	for(int j = 0; j < 4; ++j)
		for(int i = 0; i < 4; ++i)
			part.transform[j][i] = m[i][j];

	// draw all meshes assigned to this node
	for(unsigned int n = 0; n < node->mNumMeshes; ++n){
		const struct aiMesh *mesh = scene->mMeshes[node->mMeshes[n]];

		part.first = vertex_data.size() / 3;
		part.count = mesh->mNumFaces * 3;

		//Allocate data
		vertex_data.reserve(vertex_data.size() + part.count * 3);
		if(mesh->HasNormals())
			normal_data.reserve(normal_data.size() + part.count * 3);
		if(mesh->mColors[0] != nullptr)
			color_data.reserve(color_data.size() + part.count * 4);
		if(mesh->mTextureCoords[0] != nullptr)
			uv_data.reserve(uv_data.size() + part.count * 2);

		if(mesh->HasNormals() && mesh->mTextureCoords[0] != nullptr){
			tangent_data.reserve(tangent_data.size() + part.count * 3);
			binormal_data.reserve(binormal_data.size() + part.count * 3);
		}

		//Add the vertices from file
		for(unsigned int t = 0; t < mesh->mNumFaces; ++t){
			const struct aiFace *face = &mesh->mFaces[t];

			if(face->mNumIndices != 3)
				THROW_EXCEPTION("Only triangle meshes are supported");

			// storage to calculate the current face's binormals and tangents
			std::vector<glm::vec3> face_vertices, face_normals;
			std::vector<glm::vec2> face_uvs;

			for(unsigned int i = 0; i < face->mNumIndices; i++){
				const int index = face->mIndices[i];
				const auto v = mesh->mVertices[index];
				face_vertices.push_back(glm::vec3{ v.x,v.y,v.z });

				vertex_data.push_back(v.x);
				vertex_data.push_back(v.y);
				vertex_data.push_back(v.z);

				if(mesh->HasNormals()){
					auto n = mesh->mNormals[index];
					if(invert)
						n = -n;
					face_normals.push_back(glm::vec3{ n.x,n.y,n.z });
					normal_data.push_back(n.x);
					normal_data.push_back(n.y);
					normal_data.push_back(n.z);
				}

				if(mesh->mColors[0] != nullptr){
					color_data.push_back(mesh->mColors[0][index].r);
					color_data.push_back(mesh->mColors[0][index].g);
					color_data.push_back(mesh->mColors[0][index].b);
					color_data.push_back(mesh->mColors[0][index].a);
				}
				if(mesh->mTextureCoords[0] != nullptr){
					auto uv = mesh->mTextureCoords[0][index];
					face_uvs.push_back(glm::vec2{ uv.x, uv.y });
					uv_data.push_back(uv.x);
					uv_data.push_back(uv.y);
				}

				// the model is loaded by ASSIMP with the aiProcess_CalcTangentSpace flag
				// so the tangents and binormals (bitangents) are calculated for us
				if(mesh->mTangents != nullptr){
					tangent_data.push_back(mesh->mTangents[index].x);
					tangent_data.push_back(mesh->mTangents[index].y);
					tangent_data.push_back(mesh->mTangents[index].z);

					// as per description, if mTangents is filled, so is mBitangents
					binormal_data.push_back(mesh->mBitangents[index].x);
					binormal_data.push_back(mesh->mBitangents[index].y);
					binormal_data.push_back(mesh->mBitangents[index].z);
				}
			}
		}
	}

	// load all children
	for(unsigned int n = 0; n < node->mNumChildren; ++n){
		part.children.push_back(MeshPart());
		loadRecursive(part.children.back(), invert, vertex_data, normal_data, color_data, uv_data, tangent_data,
		              binormal_data, scene, node->mChildren[n]);
	}
}

GLuint Model::loadTexture(std::string filename){
	std::vector<unsigned char> data;
	ILuint ImageName;
	unsigned int width, height;
	GLuint texture;

	ilGenImages(1, &ImageName); // Grab a new image name.
	ilBindImage(ImageName);

	if(!ilLoadImage(filename.c_str())){
		ILenum e;
		std::stringstream error;
		while((e = ilGetError()) != IL_NO_ERROR){
			error << e << ": " << iluErrorString(e) << std::endl;
		}
		ilDeleteImages(1, &ImageName); // Delete the image name. 
		throw std::runtime_error(error.str());
	}

	width = ilGetInteger(IL_IMAGE_WIDTH); // getting image width
	height = ilGetInteger(IL_IMAGE_HEIGHT); // and height
	data.resize(width * height * 3);

	ilCopyPixels(0, 0, 0, width, height, 1, IL_RGB, IL_UNSIGNED_BYTE, data.data());
	ilDeleteImages(1, &ImageName); // Delete the image name. 

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data.data());
	glBindTexture(GL_TEXTURE_2D, 0);
	CHECK_GL_ERROR();

	return texture;
}

void Model::bindDiffuseMap(GLuint texture_unit){
	glActiveTexture(GL_TEXTURE0 + texture_unit);
	glBindTexture(GL_TEXTURE_2D, diffuse_texture);
}

void Model::bindBumpMap(GLuint texture_unit){
	glActiveTexture(GL_TEXTURE0 + texture_unit);
	glBindTexture(GL_TEXTURE_2D, bump_texture);
}

void Model::bindSpecularMap(GLuint texture_unit){
	glActiveTexture(GL_TEXTURE0 + texture_unit);
	glBindTexture(GL_TEXTURE_2D, specular_texture);
}

void Model::unbindTexture(){
	glBindTexture(GL_TEXTURE_2D, 0);
}

/**
 * Compute the tangents and binormals for each vertex per face.
 * tangent_data and binormal_data are out values;
 * 
 * This is a copy of what is shown on this tutorial:
 * http://www.opengl-tutorial.org/intermediate-tutorials/tutorial-13-normal-mapping/
 */
/*void Model::compute_tangent_space(const std::vector<glm::vec3> &vertices,
                                  const std::vector<glm::vec3> &normals,
                                  const std::vector<glm::vec2> &uvs,
                                  std::vector<float> &tangent_data_out, std::vector<float> &binormal_data_out){

		auto &v0 = vertices[0];
		auto &v1 = vertices[1];
		auto &v2 = vertices[2];

		auto &uv0 = uvs[0];
		auto &uv1 = uvs[1];
		auto &uv2 = uvs[2];


		// position deltas (edges)
		glm::vec3 edge1{ v1 - v0 };
		glm::vec3 edge2{ v2 - v0 };

		// UV delta ("texture span")
		glm::vec2 deltaUV1{ uv1 - uv0  };
		glm::vec2 deltaUV2{ uv2 - uv0  };

		float UVdet = deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x;
		float r = 1.0f / UVdet;

		glm::vec3 tangent = (edge1 * deltaUV2.y - edge2 * deltaUV1.y) * r;
		glm::vec3 binormal = (edge2 * deltaUV1.x - edge1 * deltaUV2.x) * r;


		for(int i = 0; i < 3; i++){
			glm::vec3 n{ normals[i] };
			auto t = tangent;
			// Gram-Schmidt orthogonalize
			t = glm::normalize(t - n * glm::dot(n, t));

			// Calculate handedness
			if (glm::dot(glm::cross(n, t), binormal) < 0.0f) {
				t = t * -1.0f;
			}

			tangent_data_out.push_back(t.x);
			tangent_data_out.push_back(t.y);
			tangent_data_out.push_back(t.z);

			binormal_data_out.push_back(binormal.x);
			binormal_data_out.push_back(binormal.y);
			binormal_data_out.push_back(binormal.z);			
		}

}*/
