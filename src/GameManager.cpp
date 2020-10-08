#include "GameManager.h"
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <assert.h>
#include <stdexcept>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/transform2.hpp>
#include "GLUtils/DebugOutput.hpp"
#include <IL/il.h>
#include <IL/ilut.h>

using std::cerr;
using std::endl;
using GLUtils::VBO;
using GLUtils::Program;
using GLUtils::readFile;


GameManager::GameManager(){
	fps_timer.restart();

	render_mode = RENDERMODE_PHONG;
	zoom = 1;
	LOD = 1.f;
	near_plane = 0.5f;
	far_plane = 30.0f;
	fovy = 45.0f;
	light.position = glm::vec3(10, 0, 0);
}

GameManager::~GameManager(){}

void GameManager::init() {
	// Initialize SDL
	if (SDL_Init(SDL_INIT_EVERYTHING) < 0) {
		std::stringstream err;
		err << "Could not initialize SDL: " << SDL_GetError();
		THROW_EXCEPTION(err.str());
	}
	atexit(SDL_Quit);

	ilInit();
	iluInit();

	createOpenGLContext();
	setOpenGLStates();
	createMatrices();
	createSimpleProgram();
	createVAO();
}

void GameManager::createOpenGLContext(){
	//Set OpenGL major an minor versions
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

	// Set OpenGL attributes
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1); // Use double buffering
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16); // Use framebuffer with 16 bit depth buffer
	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8); // Use framebuffer with 8 bit for red
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8); // Use framebuffer with 8 bit for green
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8); // Use framebuffer with 8 bit for blue
	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8); // Use framebuffer with 8 bit for alpha
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

	// Initalize video
	main_window = SDL_CreateWindow("Westerdals - PG6200 Reworked Template", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
	                               window_width, window_height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
	if(!main_window){
		THROW_EXCEPTION("SDL_CreateWindow failed");
	}

	//Create OpenGL context
	main_context = SDL_GL_CreateContext(main_window);

	// Init glew
	glewExperimental = GL_TRUE;
	GLenum glewErr = glewInit();
	if(glewErr != GLEW_OK){
		std::stringstream err;
		err << "Error initializing GLEW: " << glewGetErrorString(glewErr);
		THROW_EXCEPTION(err.str());
	}

	//enabling KHR debugging
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(GLDEBUGPROC(GLUtils::DebugOutput::myCallback), nullptr);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);

	// Lets do the ugly thing of swallowing the error....
	glGetError();

	cam_trackball.setWindowSize(window_width, window_height);
}

void GameManager::setOpenGLStates(){
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glEnable(GL_CULL_FACE);
	glClearColor(0.5, 0.5, 0.5, 1.0);
	glViewport(0, 0, window_width, window_height);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	// initialise number of vertices per patch for tessellation
	glPatchParameteri(GL_PATCH_VERTICES, 3);

	CHECK_GL_ERRORS();
}

void GameManager::createMatrices(){
	model_matrix = scale(glm::mat4(1.0f), glm::vec3(3));

	camera.projection = glm::perspective(fovy / zoom, window_width / (float)window_height, near_plane, far_plane);
	camera.view = translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));

}

void GameManager::createSimpleProgram(){
	std::string fs_src = readFile("shaders/basic_phong.frag");
	std::string tcs_src = readFile("shaders/basic_phong.tcs");
	std::string tes_src = readFile("shaders/basic_phong.tes");
	std::string vs_src = readFile("shaders/basic_phong.vert");

	program.reset(new Program(vs_src, tcs_src, tes_src, fs_src));

	//Set uniforms for the program.
	program->use();
	glUniform1i(program->getUniform("diffuse_texture"), DIFFUSE_TEX);
	glUniform1i(program->getUniform("specular_texture"), SPECULAR_TEX);
	glUniform1i(program->getUniform("normal_texture"), NORMAL_TEX);
	CHECK_GL_ERROR();
	program->disuse();
	
}

void GameManager::createVAO(){
	glGenVertexArrays(1, &main_scene_vao[0]);
	glBindVertexArray(main_scene_vao[0]);
	CHECK_GL_ERROR();

	// Seperate VBOs
	model.reset(new Model("models/ico-sphere.obj", false));
//	model.reset(new Model("models/low_poly_ico_sphere.obj", false));
	model->getVertices()->bind();
	program->setAttributePointer("position", 3);
	CHECK_GL_ERROR();
	model->getNormals()->bind();
	program->setAttributePointer("normal", 3);
	CHECK_GL_ERROR();
	model->getUVs()->bind();
	program->setAttributePointer("UV", 2);
	CHECK_GL_ERROR();

	model->getTangents()->bind();
	program->setAttributePointer("tangent", 3);
	CHECK_GL_ERROR();
	model->getBinormals()->bind();
	program->setAttributePointer("binormal", 3);
	CHECK_GL_ERROR();

	glBindVertexArray(0);
	CHECK_GL_ERROR();
}


void GameManager::renderMeshRecursive(MeshPart &mesh, const std::shared_ptr<Program> &program,
                                      const glm::mat4 &view_matrix, const glm::mat4 &model_matrix,
                                      glm::mat4 &projection_matrix){
	//Create modelview matrix
	const glm::mat4 meshpart_model_matrix = model_matrix * mesh.transform;
	glm::mat4 model_view_mat = view_matrix * meshpart_model_matrix;
	glm::mat3 model_view_mat_3x3 = glm::mat3(model_view_mat);
	
	//3x3 leading submatrix of the modelview matrix for the TBN matrix in the vertex shader
	glm::mat3 normal_matrix = transpose(inverse(glm::mat3(model_view_mat)));

	program->use();

	glUniformMatrix4fv(program->getUniform("model_view_mat"), 1, 0, value_ptr(model_view_mat));
	glUniformMatrix4fv(program->getUniform("model_mat"), 1, 0, value_ptr(meshpart_model_matrix));
	glUniformMatrix3fv(program->getUniform("model_view_mat_3x3"), 1, 0, value_ptr(model_view_mat_3x3));
	glUniformMatrix4fv(program->getUniform("proj_mat"), 1, 0, value_ptr(projection_matrix));
	glUniformMatrix3fv(program->getUniform("normal_mat"), 1, 0, value_ptr(normal_matrix));
	// glm::mat4 view_projection_matrix =  projection_matrix * view_matrix;
	// glUniformMatrix4fv(program->getUniform("view_proj_mat"), 1, 0, value_ptr(view_projection_matrix));

	glDrawArrays(GL_PATCHES, mesh.first, mesh.count);

	for(int i = 0; i < (int)mesh.children.size(); ++i)
		renderMeshRecursive(mesh.children.at(i), program, view_matrix, meshpart_model_matrix, projection_matrix);

	program->disuse();
}



void GameManager::render(){
	const float elapsed = fps_timer.elapsedAndRestart();

	const glm::mat4 rotation = rotate(elapsed, glm::vec3(0.0f, 1.0f, 0.0f));
	light.position = glm::mat3(rotation) * light.position;

	const glm::mat4 view = camera.view * cam_trackball.getTransform();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	program->use();

	glUniform3fv(program->getUniform("light_position"), 1, value_ptr(light.position));
	glUniform1i(program->getUniform("lighting"), lighting_enabled ? 1 : 0);
	glUniform1i(program->getUniform("debugSwitch"), debugSwitch ? 1 : 0);
	glUniform1i(program->getUniform("distance_LOD_enabled"), distance_LOD_enabled ? 1 : 0);
	glUniform1f(program->getUniform("TessLevel"), LOD);

	model->bindDiffuseMap(DIFFUSE_TEX);
	model->bindSpecularMap(SPECULAR_TEX);
	model->bindBumpMap(NORMAL_TEX);

	//Render geometry
	glBindVertexArray(main_scene_vao[0]);
	switch(render_mode){
		case RENDERMODE_PHONG:
			glCullFace(GL_BACK);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			break;
		case RENDERMODE_WIREFRAME:
			glCullFace(GL_BACK);
			glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			lighting_enabled = true;
			break;
		default:
			THROW_EXCEPTION("Rendermode not supported");
	}

	renderMeshRecursive(model->getMesh(), program, view, model_matrix, camera.projection);

	glBindVertexArray(0);
	CHECK_GL_ERROR();
}

void GameManager::move_ball(float zOffset){
	auto view_with_newZ = translate(camera.view, glm::vec3(0.0, 0.0, zOffset));
	view_with_newZ[3][2] = glm::clamp(view_with_newZ[3][2] + zOffset, -26.f, -2.f);
	camera.view = view_with_newZ;
}

void GameManager::display_commands(){
	std::cout << "\n\nPG6200 Assignment 2 - Normal maps and tessellation\n\n";
	std::cout << "== Movement ==\n";
	std::cout << "[w]: move towards the ball\n";
	std::cout << "[s]: move away from the ball\n";
	std::cout << "Mouse wheel up: zoom in (does not affect LOD)\n";
	std::cout << "Mouse wheel down: zoom out (does not affect LOD)\n";
	std::cout << "Rotate the ball by holding LMB and moving the mouse.\n\n";

	std::cout << "== Options ==\n";
	std::cout << "[L] toggle Blinn-Phong light reflection + normal mapping\n";
	std::cout << "[Z] switch between LOD modes: by distance or manual\n";
	std::cout << "[+ / -] increases / decreases the LOD under manual LOD mode\n";
	std::cout << "[Space] toggle coloring by barycentric coordinate per face\n\n";

	std::cout << "== Render modes ==\n";
	std::cout << "[2] normal (filled polygon rendering)\n";
	std::cout << "[3] wireframe" << std::endl;
}

void GameManager::zoomIn(){
	zoom = min(zoom + 10e-4f, 1.f + .015f);
	camera.projection = glm::perspective(fovy / zoom,
	                                     window_width / float(window_height), near_plane, far_plane);
}

void GameManager::zoomOut(){
	zoom = max(zoom - 10e-4f, 1.f - .015f);
	camera.projection = glm::perspective(fovy / zoom,
	                                     window_width / (float)window_height, near_plane, far_plane);
}

void GameManager::increaseLOD(){
	LOD = min(LOD + 1.f, 12.f);
}

void GameManager::decreaseLOD(){
	LOD = max(LOD - 1.f, 1.f);
}

void GameManager::play(){
	bool doExit = false;

	display_commands();

	//SDL main loop
	while(!doExit){
		SDL_Event event;
		while(SDL_PollEvent(&event)){
			// poll for pending events
			switch(event.type){
				case SDL_MOUSEWHEEL:
					if(event.wheel.y == 1)
						zoomIn();
					else if(event.wheel.y == -1)
						zoomOut();
					break;
				case SDL_MOUSEBUTTONDOWN:
					cam_trackball.rotateBegin(event.motion.x, event.motion.y);
					break;
				case SDL_MOUSEBUTTONUP:
					cam_trackball.rotateEnd(event.motion.x, event.motion.y);
					break;
				case SDL_MOUSEMOTION:
					cam_trackball.rotate(event.motion.x, event.motion.y, zoom);
					break;
				case SDL_KEYDOWN:
					switch(event.key.keysym.sym){
						case SDLK_ESCAPE:
							doExit = true;
							break;
						case SDLK_q:
							if(event.key.keysym.mod & KMOD_CTRL) doExit = true; //Ctrl+q
							break;
						case SDLK_l:
							lighting_enabled = !lighting_enabled;
							break;
						case SDLK_RIGHT:
							camera.view = translate(camera.view, glm::vec3(-0.1, 0.0, 0.0));
							break;
						case SDLK_LEFT:
							camera.view = translate(camera.view, glm::vec3(0.1, 0.0, 0.0));
							break;
						case SDLK_UP:
							camera.view = translate(camera.view, glm::vec3(0.0, -0.1, 0.0));
							break;
						case SDLK_DOWN:
							camera.view = translate(camera.view, glm::vec3(0.0, 0.1, 0.0));
							break;
						case SDLK_w:
							move_ball(0.2f);
							break;
						case SDLK_s:
							move_ball(-0.2f);
							break;
						case SDLK_z:
							distance_LOD_enabled = !distance_LOD_enabled;
							break;
						case SDLK_PLUS:
							increaseLOD();
							break;
						case SDLK_MINUS:
							decreaseLOD();
							break;
						case SDLK_SPACE:
							debugSwitch = !debugSwitch;
							break;
						case SDLK_2:
							render_mode = RENDERMODE_PHONG;
							break;
						case SDLK_3:
							render_mode = RENDERMODE_WIREFRAME;
							break;
					}
					break;
				case SDL_QUIT: //e.g., user clicks the upper right x
					doExit = true;
					break;
			}
		}

		//Render, and swap front and back buffers
		render();
		SDL_GL_SwapWindow(main_window);
	}
	quit();
}

void GameManager::quit(){
	std::cout << "Bye bye..." << endl;
}

