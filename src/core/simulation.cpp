#include "simulation.h"

Simulation::Simulation(int clothWidth, int clothHeight)
	: cloth(Cloth(clothWidth, clothHeight)) {
	triVerts = genTrisFromMesh();
}

void Simulation::update() {
}

GLfloat *Simulation::genTrisFromMesh() {
	GLfloat *tris = new GLfloat[9 * getNumTris()];

	// copy in triangles
	for (int i = 0; i < cloth.yRes - 1; i++) {
		for (int j = 0 ; j < cloth.xRes - 1; j++) {
			GLfloat *triPairStart = tris + 18 * (i*(cloth.xRes-1) + j);

			copyPoint(triPairStart,      cloth.getWorldPoint(j,   i),   3);
			copyPoint(triPairStart + 3,  cloth.getWorldPoint(j,   i+1), 3);
			copyPoint(triPairStart + 6,  cloth.getWorldPoint(j+1, i),   3);

			copyPoint(triPairStart + 9,  cloth.getWorldPoint(j+1, i),   3);
			copyPoint(triPairStart + 12, cloth.getWorldPoint(j,   i+1), 3);
			copyPoint(triPairStart + 15, cloth.getWorldPoint(j+1, i+1), 3);
		}
	}

	return tris;
}

int Simulation::getNumTris() {
	return 2 * (cloth.xRes - 1) * (cloth.yRes - 1);
}

void Simulation::copyPoint(GLfloat *dest, GLfloat *src, int dim) {
	memcpy(dest, src, dim * sizeof(GLfloat));
}
