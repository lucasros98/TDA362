

#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <stb_image.h>


#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"

#include "ParticleSystem.cpp"



using std::min;
using std::max;

///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
bool showUI = false;
int windowWidth, windowHeight;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;
GLuint particleShaderProgram;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap, irradianceMap, reflectionMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
vec3 point_light_color = vec3(1.f, 1.f, 1.f);

float point_light_intensity_multiplier = 10000.0f;


///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 10.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model* fighterModel = nullptr;
labhelper::Model* landingpadModel = nullptr;
labhelper::Model* sphereModel = nullptr;

mat4 roomModelMatrix;
mat4 landingPadModelMatrix;
mat4 fighterModelMatrix;

mat4 T(3.0f), R(1.0f);


///////////////////////////////////////////////////////////////////////////////
// Particle stuff
///////////////////////////////////////////////////////////////////////////////
GLuint vertexArrayObject;
GLuint positionBuffer;
GLuint texBuffer;

ParticleSystem particle_system(100000);

float particleOffset;

void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag",
	                                             is_reload);
	if (shader != 0)
		particleShaderProgram = shader;
	shader = labhelper::loadShaderProgram("../project/background.vert", "../project/background.frag",
		is_reload);

	if(shader != 0)
		backgroundProgram = shader;

	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if(shader != 0)
		shaderProgram = shader;
}

void initGL()
{
	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	backgroundProgram = labhelper::loadShaderProgram("../project/background.vert",
	                                                 "../project/background.frag");
	shaderProgram = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag");
	simpleShaderProgram = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag");

	particleShaderProgram = labhelper::loadShaderProgram("../project/particle.vert", "../project/particle.frag");

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	fighterModel = labhelper::loadModelFromOBJ("../scenes/NewShip.obj");
	landingpadModel = labhelper::loadModelFromOBJ("../scenes/landingpad.obj");
	sphereModel = labhelper::loadModelFromOBJ("../scenes/sphere.obj");

	roomModelMatrix = mat4(1.0f);
	fighterModelMatrix = translate(15.0f * worldUp);
	landingPadModelMatrix = mat4(1.0f);

	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	const int roughnesses = 8;
	std::vector<std::string> filenames;
	for(int i = 0; i < roughnesses; i++)
		filenames.push_back("../scenes/envmaps/" + envmap_base_name + "_dl_" + std::to_string(i) + ".hdr");

	reflectionMap = labhelper::loadHdrMipmapTexture(filenames);
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");
	irradianceMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + "_irradiance.hdr");

	///////////////////////////////////////////////////////////////////////////
	// Particle system
	///////////////////////////////////////////////////////////////////////////
	glGenVertexArrays(1, &vertexArrayObject);
	// Set it as current, i.e., related calls will affect this object
	glBindVertexArray(vertexArrayObject);

	glGenBuffers(1, &positionBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);

	glBufferData(GL_ARRAY_BUFFER, sizeof(vec4) * 100000, nullptr, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 4, GL_FLOAT, false /*normalized*/, 0 /*stride*/, 0 /*offset*/);
	// Enable the attribute
	glEnableVertexAttribArray(0);


	////////////////////////////
	// TEXTURES
	///////////////////////////
	float texcoords1[] = {
		0.0f, 1.0f,    // (u,v) for v0
		0.0f, 0.0f,   // (u,v) for v1
		1.0f, 1.0f,   // (u,v) for v2
		1.0f, 0.0f     // (u,v) for v3
	};

	glGenBuffers(1, &texBuffer);
	glBindBuffer(GL_ARRAY_BUFFER, texBuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(texcoords1), texcoords1, GL_STATIC_DRAW);

	glVertexAttribPointer(2, 2, GL_FLOAT, false/*normalized*/, 0/*stride*/, 0/*offset*/);

	glEnableVertexAttribArray(2);

	//Task 6
	int w, h, comp;
	unsigned char* image = stbi_load("../scenes/explosion.png", &w, &h, &comp, STBI_rgb_alpha);

	glGenTextures(1, &texBuffer);
	glBindTexture(GL_TEXTURE_2D, texBuffer);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	free(image);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glGenerateMipmap(GL_TEXTURE_2D);
	// Sets the type of filtering to be used on magnifying and
	// minifying the active texture. These are the nicest available options.
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);



	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling
}

void debugDrawLight(const glm::mat4& viewMatrix,
                    const glm::mat4& projectionMatrix,
                    const glm::vec3& worldSpaceLightPos)
{
	mat4 modelMatrix = glm::translate(worldSpaceLightPos);
	glUseProgram(shaderProgram);
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * modelMatrix);
	labhelper::render(sphereModel);
}


void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::drawFullScreenQuad();
}

void drawScene(GLuint currentShaderProgram,
               const mat4& viewMatrix,
               const mat4& projectionMatrix,
               const mat4& lightViewMatrix,
               const mat4& lightProjectionMatrix)
{
	glUseProgram(currentShaderProgram);
	// Light source
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
	                          point_light_intensity_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
	                          normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));


	// Environment
	labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);

	// camera
	labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

	// landing pad
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * landingPadModelMatrix)));

	labhelper::render(landingpadModel);

	// Fighter
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * fighterModelMatrix)));


	labhelper::render(fighterModel);
}


void display(void)
{
	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 2000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE7);
	glBindTexture(GL_TEXTURE_2D, irradianceMap);
	glActiveTexture(GL_TEXTURE8);
	glBindTexture(GL_TEXTURE_2D, reflectionMap);
	glActiveTexture(GL_TEXTURE0);


	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);



	drawBackground(viewMatrix, projMatrix);
	drawScene(shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix);
	debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));

	

	unsigned int active_particles = particle_system.particles.size();
	/* Code for extracting data goes here */
	std::vector<glm::vec4> data;
	for each (Particle particle in particle_system.particles)
	{
		//transform to viewspace
		vec4 viewSpacePos;
		viewSpacePos = viewMatrix * vec4(particle.pos,1.0f);
		data.push_back(vec4(vec3(viewSpacePos), particle.lifetime / particle.life_length));
	}
	// sort particles with sort from c++ standard library
	std::sort(data.begin(), std::next(data.begin(), active_particles),
		[](const vec4& lhs, const vec4& rhs) { return lhs.z < rhs.z; });

	// Set it as current, i.e., related calls will affect this object
	glUseProgram(particleShaderProgram);
	labhelper::setUniformSlow(particleShaderProgram, "P",
		projMatrix);
	labhelper::setUniformSlow(particleShaderProgram, "screen_x", float(windowWidth));
	labhelper::setUniformSlow(particleShaderProgram, "screen_y", float(windowHeight));


	glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vec4) * active_particles, data.data());

	glBindVertexArray(vertexArrayObject);

	// Enable shader program point size modulation.
	glEnable(GL_PROGRAM_POINT_SIZE);
	// Enable blending.
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texBuffer);

	glDrawArrays(GL_POINTS, 0, active_particles);


}

bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while(SDL_PollEvent(&event))
	{
		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			showUI = !showUI;
		}
		if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
		   && (!showUI || !ImGui::GetIO().WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if(!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.1f;
			mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
			mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
			                    normalize(cross(cameraDirection, worldUp)));
			cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);
	vec3 cameraRight = cross(cameraDirection, worldUp);

	if(state[SDL_SCANCODE_W])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_S])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_A])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_D])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_Q])
	{
		cameraPosition -= cameraSpeed * deltaTime * worldUp;
	}
	if(state[SDL_SCANCODE_E])
	{
		cameraPosition += cameraSpeed * deltaTime * worldUp;
	}


	// implement controls based on key states
	const float speed = 10.f;
	const float rotateSpeed = 2.f;
	if (state[SDL_SCANCODE_UP]) {
		fighterModelMatrix = translate(fighterModelMatrix,vec3(-1.0f, 0.0f, 0.0f));

		///////////////////////////////////////////////////////////////////////////
		// Particle system
		///////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < 64; i++) {
			Particle p;
			p.pos = translate(fighterModelMatrix, vec3(15, 3, 0))[3];
			p.lifetime = 0;
			p.life_length = 5;

			const float theta = labhelper::uniform_randf(0.f, 2.f * M_PI);
			//const float u = labhelper::uniform_randf(-1.f, 1.f);
			const float u = labhelper::uniform_randf(0.95f, 1.f);

			glm::vec3 vel = glm::vec3(u , sqrt(1.f - u * u) * cosf(theta), sqrt(1.f - u * u) * sinf(theta));
			//set velocity
			p.velocity = vec3(fighterModelMatrix * vec4(vel, 0.0))*100.f;
			particle_system.spawn(p);
		}

	}
	if (state[SDL_SCANCODE_RIGHT]) {
		fighterModelMatrix = rotate(fighterModelMatrix,.1f, vec3(0.0f, -.1f, 0.0f));
	}
	if (state[SDL_SCANCODE_LEFT]) {
		//T[3] += speed * deltaTime * vec4(1.0f, 0.0f, 0.0f, 0.0f);
		fighterModelMatrix = rotate(fighterModelMatrix, .1f, vec3(0.0f, .1f, 0.0f));
	}





	return quitEvent;
}

void gui()
{
	// Inform imgui of new frame
	ImGui_ImplSdlGL3_NewFrame(g_window);

	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// ----------------------------------------------------------
	ImGui::DragFloat3("Offset",&particleOffset);

	// Render the GUI.
	ImGui::Render();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

	initGL();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;
		// render to window
		display();

		particle_system.process_particles(deltaTime);

		// Render overlay GUI.
		if(showUI)
		{
			gui();
		}

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);

		// check events (keyboard among other)
		stopRendering = handleEvents();
	}
	// Free Models
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(sphereModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
