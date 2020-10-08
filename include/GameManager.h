#ifndef _GAMEMANAGER_H_
#define _GAMEMANAGER_H_

#include <memory>
#include <map>

#include <GL/glew.h>
#include <SDL.h>
#include <glm/glm.hpp>

#include "Timer.h"
#include "GLUtils/GLUtils.hpp"
#include "Model.h"
#include "VirtualTrackball.h"

/**
 * This class handles the game logic and display.
 * Uses SDL as the display manager, and glm for 
 * vector and matrix computations
 */
class GameManager{
public:

	/**
	 * Constructor
	 */
	GameManager();

	/**
	 * Destructor
	 */
	~GameManager();

	/**
	 * Initializes the game, including the OpenGL context
	 * and data required
	 */
	void init();

	void move_ball(float zOffset);
	void display_commands();
	/**
	 * The main loop of the game. Runs the SDL main loop
	 */
	void play();

	/**
	 * Quit function
	 */
	void quit();

	/**
	 * Function that handles rendering into the OpenGL context
	 */
	void render();

protected:
	/**
	 * Creates the OpenGL context using SDL
	 */
	void createOpenGLContext();

	/*
	*
	*/
	void initSDL();

	/**
	 * Sets states for OpenGL that we want to keep persistent
	 * throughout the game
	 */
	static void setOpenGLStates();

	/**
	 * Creates the matrices for the OpenGL transformations,
	 * perspective, etc.
	 */
	void createMatrices();

	/**
	 * Compiles, attaches, links, and sets uniforms for
	 * a simple OpenGL program
	 */
	void createSimpleProgram();

	/**
	 * Creates vertex array objects
	 */
	void createVAO();

	static const unsigned int window_width = 800;
	static const unsigned int window_height = 600;


	float near_plane;
	float far_plane;
	float fovy;


	bool debugSwitch = false;
	bool lighting_enabled = true;
	bool distance_LOD_enabled;

private:
	enum RenderMode{
		RENDERMODE_PHONG,
		RENDERMODE_WIREFRAME,
		RENDERMODE_HIDDEN_LINE,
		RENDERMODE_FLAT,
	};

	enum TextureShaderLayoutIndex{
		DIFFUSE_TEX,
		NORMAL_TEX,
		SPECULAR_TEX
	};

	void increaseLOD();
	void decreaseLOD();
	void zoomIn();
	void zoomOut();

	static void renderMeshRecursive(MeshPart &mesh, const std::shared_ptr<GLUtils::Program> &program,
	                                const glm::mat4 &modelview, const glm::mat4 &transform,
	                                glm::mat4 &projection_matrix);

	SDL_Window *main_window; 
	SDL_GLContext main_context; 
	RenderMode render_mode;

	GLuint main_scene_vao[1]; //< number of different "collection" of vbo's we have

	std::map<std::string, std::shared_ptr<Model>> models;
	std::map<std::string, std::shared_ptr<GLUtils::Program>> shaders;


	float zoom;
	float LOD;
	Timer fps_timer;
	VirtualTrackball cam_trackball;

	struct{
		glm::vec3 position;
	} light;

	struct{
		glm::mat4 projection;
		glm::mat4 view;
	} camera;

	std::shared_ptr<Model> model;
	std::shared_ptr<GLUtils::Program> program;
	glm::mat4 model_matrix; 
};

#endif // _GAMEMANAGER_H_
