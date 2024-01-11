#include "WaterMPM.h"

int WaterMPM::Allocate(unsigned sx, unsigned sy, unsigned sz) {
	massGrid.Allocate(sx+1, sy+1, sz+1);
	velocityGrid.Allocate(sx+1, sy+1, sz+1);

	return 0;
}

int WaterMPM::Step() {
	ParticlesToGrid();
	AdvanceGrid();
	GridToParticles();
	AdvanceParticles();

	return 0;
}

int WaterMPM::ParticlesToGrid() {
	for (int i = 0; i < particles.size(); ++i) {
		Particle& particle = particles[i];
		
		Vec3i minCoord(
			floor(particle.position[0] / h - 0.5f),
			floor(particle.position[1] / h - 0.5f),
			floor(particle.position[2] / h - 0.5f)
		); // min index in the grid;

		Vec3f positionInCell(
			particle.position[0] / h - minCoord[0],
			particle.position[1] / h - minCoord[1],
			particle.position[2] / h - minCoord[2]
		); // in range [0, 1]


	}

	return 0;
}

int WaterMPM::AdvanceGrid() {
	return -1;
}

int WaterMPM::GridToParticles() {
	return -1;
}

int WaterMPM::AdvanceParticles() {
	return -1;
}