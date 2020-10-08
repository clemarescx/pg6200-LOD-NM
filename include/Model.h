#ifndef _MODEL_H__
#define _MODEL_H__

#include <memory>
#include <string>
#include <vector>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "GLUtils/VBO.hpp"

struct MeshPart{
	MeshPart() : first(0), count(0){}
	glm::mat4 transform;
	unsigned int first;
	unsigned int count;
	std::vector<MeshPart> children;
};

class Model{
public:
	Model(std::string filename, bool invert = false);
	~Model();

	MeshPart getMesh(){ return root; }
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> getVertices(){ return vertices; }
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> getNormals(){ return normals; }
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> getColors(){ return colors; }
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> getUVs(){ return uvs; }
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> getTangents(){ return tangents; }
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> getBinormals(){ return binormals; }
	void bindDiffuseMap(GLuint texture_unit);
	void bindBumpMap(GLuint texture_unit);
	void bindSpecularMap(GLuint texture_unit);
	void unbindTexture();

private:
	static void compute_tangent_space(const std::vector<glm::vec3> &face_vertices,
	                                  const std::vector<glm::vec3> &face_normals,
	                                  const std::vector<glm::vec2> &face_uvs,
	                                  std::vector<float> &tangent_data_out,
	                                  std::vector<float> &binormal_data_out);
	static void loadRecursive(MeshPart &part, bool invert,
	                          std::vector<float> &vertex_data, std::vector<float> &normal_data,
	                          std::vector<float> &color_data, std::vector<float> &uv_data,
	                          std::vector<float> &tangent_data, std::vector<float> &binormal_data,
	                          const aiScene *scene,
	                          const aiNode *node);
	GLuint loadTexture(std::string filename);

	static void findBBoxRecursive(const aiScene *scene, const aiNode *node, glm::vec3 &min_dim, glm::vec3 &max_dim,
	                              aiMatrix4x4 *trafo);

	const aiScene *scene;
	MeshPart root;

	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> normals;
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> vertices;
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> colors;
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> uvs;
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> tangents;
	std::shared_ptr<GLUtils::VBO<GL_ARRAY_BUFFER>> binormals;

	glm::vec3 min_dim;
	glm::vec3 max_dim;

	unsigned int n_vertices;
	GLuint diffuse_texture, bump_texture, specular_texture;
};

#endif
