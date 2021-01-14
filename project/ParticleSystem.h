#pragma once


#include <GL/glew.h>
#include <vector>
#include <glm/detail/type_vec3.hpp>
#include <glm/mat4x4.hpp>

struct Particle
{
	float lifetime;
	float life_length;
	glm::vec3 velocity;
	glm::vec3 pos;
};

class ParticleSystem
{
public:
	// Members
	std::vector<Particle> particles;
	int max_size;
	// Ctor/Dtor
	ParticleSystem() : max_size(0)
	{
	}
	explicit ParticleSystem(int size) : max_size(size)
	{
	}
	~ParticleSystem()
	{
	}
	// Methods
	void kill(int id) {
		Particle last = particles.back();
		particles[id] = last;
		particles.pop_back();
	}

	void spawn(Particle particle) {
		if(particles.size() < max_size)
			particles.push_back(particle);
	}

	void process_particles(float dt) {
		for (unsigned i = 0; i < particles.size(); ++i) {
			// Kill dead particles!
			if (particles[i].lifetime + dt > particles[i].life_length) {
				kill(i);
			}
		}
		for (unsigned i = 0; i < particles.size(); ++i) {
			// Update alive particles!
			particles[i].lifetime += dt;
			particles[i].pos += particles[i].velocity * dt;
		}
	}

};
