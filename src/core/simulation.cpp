#include "simulation.h"

using namespace std;
Simulation::Simulation(int clothXRes, int clothYRes)
	: cloth(Cloth(clothXRes, clothYRes)) {
	reset();

	triVerts = genTrisFromMesh();

	forces.resize(1, cloth.xRes * cloth.yRes);
	forcePartialX.resize(3 * cloth.xRes * cloth.yRes,
	                     3 * cloth.xRes * cloth.yRes);
}

void Simulation::update() {
	// if simulation is paused, don't update
	if (!running) return;

	// zero forces matrix
	for (int pt = 0; pt < cloth.xRes * cloth.yRes; pt++) {
		forces(0, pt)[0] = 0;
		forces(0, pt)[1] = 0;
		forces(0, pt)[2] = 0;
	}

	forcePartialX.setZero();
	//forcePartialX.reserve(81 * getNumTris());

	// get condition-forces 
	for (int i = 0; i < cloth.yRes-1; i++) {
		for (int j = 0; j < cloth.xRes-1; j++) {
			int offset = i * cloth.xRes + j;
			handleScaleCondition(offset);
			handleShearCondition(offset);
			handleBendCondition (offset);
		}
	}

	// update velocities by condition-forces
	for (int i = 0; i < cloth.xRes * cloth.yRes; i++) {
		for (int j = 0; j < 3; j++) {
			cloth.worldVels[i*3 + j] += forces(0, i)[j] *
			                            cloth.massPerVertI;
		}
	}

	// lock the top row of points, if we've enabled that setting
	int lastPoint = 3 * cloth.xRes * cloth.yRes;
	if (LOCK_TOP_POINTS) lastPoint -= 3 * cloth.xRes;

	// move the points by their velocities
	for (int i = 0; i < lastPoint; i++) {
		cloth.worldPoints[i] += cloth.worldVels[i];
	}

	// generate new triangles from the mesh
	triVerts = genTrisFromMesh();
}

void Simulation::reset() {
	// regenerate cloth
	cloth = Cloth(cloth.xRes, cloth.yRes);

	// perturb cloth for the test case
	random_device rd;
	mt19937 gen(rd());
	uniform_real_distribution<> dis(0, 1);
	for (int i = 0; i < cloth.yRes - 1; i++) {
		for (int j = 0; j < cloth.xRes; j++) {
			cloth.getWorldPoint(j, i)[0] += dis(gen) / 80;
			cloth.getWorldPoint(j, i)[1] += dis(gen) / 80;
			cloth.getWorldPoint(j, i)[2] += dis(gen) / 80;
		}
	}

	// regenerate triangles from the mesh
	triVerts = genTrisFromMesh();
}

void Simulation::handleScaleCondition(int offset) {
	int botLeftTri[3]  = {offset, offset + 1, offset + cloth.xRes};
	int topRightTri[3] = {offset + cloth.xRes, offset + 1,
	                offset + cloth.xRes + 1};

	scaleHelper(botLeftTri, true);
	scaleHelper(topRightTri, false);
}

void Simulation::scaleHelper(int *triPts, bool isBl) {
	auto condX = scaleXCondition(cloth, triPts, isBl);
	auto condY = scaleYCondition(cloth, triPts, isBl);
	for (int i = 0; i < 3; i++) {
		int ptI = triPts[i];

		// acount for scaling force
		auto partialIX = scaleXPartial(cloth, ptI, triPts, isBl);
		auto partialIY = scaleYPartial(cloth, ptI, triPts, isBl);
		auto force = -SCALE_STIFF * (partialIX.transpose() * condX +
		                             partialIY.transpose() * condY);
		forces(0, ptI) += force;

		// account for damping force
		auto velI = Vector3d(cloth.getWorldVel(ptI));
		auto dampForce = -DAMP_STIFF *
		                 (partialIX.transpose() * partialIX +
		                  partialIY.transpose() * partialIY) * velI;
		forces(0, ptI) += dampForce;

		// calculate the partial force partial x
		for (int j = 0; j < 3; j++) {
			int ptJ = triPts[j];

			auto partialJX = scaleXPartial(cloth, ptJ, triPts, isBl);
			Matrix3d pfpxX = partialIX.transpose() * partialJX +
			          scaleXSecondPartial(cloth, ptI, ptJ, triPts, isBl) *
			          condX;

			auto partialJY = scaleYPartial(cloth, ptJ, triPts, isBl);
			Matrix3d pfpxY = partialIY.transpose() * partialJY +
			          scaleYSecondPartial(cloth, ptI, ptJ, triPts, isBl) *
			          condY;

			Matrix3d pfpx = pfpxX + pfpxY;
			forcePartialX.block(i*3, j*3, 3, 3) += pfpx;
			/*
			for (int rOff = 0; rOff < 3; rOff++) {
				for (int cOff = 0; cOff < 3; cOff++) {
					forcePartialX.coeffRef(ptI*3 + rOff, ptJ*3 + cOff) +=
						pfpx(rOff, cOff);
				}
			}
			*/
		}
	}
}

void Simulation::handleShearCondition(int offset) {
	int botLeftTri[3] = {offset, offset + 1, offset + cloth.xRes};
	int topRightTri[3] = {offset + cloth.xRes, offset + 1,
			              offset + cloth.xRes + 1};

	shearHelper(botLeftTri, true);
	shearHelper(topRightTri, false);
}

void Simulation::shearHelper(int *triPts, bool isBl) {
	auto cond = shearCondition(cloth, triPts, isBl);

	for (int i = 0; i < 3; i++) {
		int ptI = triPts[i];

		auto partialI = shearPartial(cloth, ptI, triPts, isBl);
		auto force = -SHEAR_STIFF * partialI.transpose() * cond;
		forces(0, ptI) += force;
	}
}

void Simulation::handleBendCondition(int offset) {
	int xOff = offset % cloth.xRes;
	int yOff = offset / cloth.xRes;

	// diagonal triangle pair
	int diagPts[4] = {
		offset,
		offset + 1,
		offset + cloth.xRes,
		offset + cloth.xRes + 1
	};

	// right-side triangle pair
	int rightPts[4] = {
		offset + cloth.xRes,
		offset + 1,
		offset + cloth.xRes + 1,
		offset + 2
	};

	// top-side triangle pair
	int topPts[4] = {
		offset + 1,
		offset + cloth.xRes + 1,
		offset + cloth.xRes,
		offset + 2 * cloth.xRes
	};

	bendHelper(diagPts);

	if (xOff < cloth.xRes - 2)
		bendHelper(rightPts);

	if (yOff < cloth.yRes - 2)
		bendHelper(topPts);
}

void Simulation::bendHelper(int *tris) {
	auto cond = bendCondition(cloth, tris);

	for (int i = 0; i < 4; i++) {
		int ptI = tris[i];

		auto partialI = bendPartial(cloth, ptI, tris);
		auto force = -BEND_STIFF * partialI.transpose() * cond;
		forces(0, ptI) += force;
	}
}

double *Simulation::genTrisFromMesh() {
	double *tris = new double[9 * getNumTris()];

	// copy in triangles
	for (int i = 0; i < cloth.yRes - 1; i++) {
		for (int j = 0 ; j < cloth.xRes - 1; j++) {
			double *triPairStart = tris + 18 * (i*(cloth.xRes-1) + j);
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

void Simulation::copyPoint(double *dest, double *src, int dim) {
	memcpy(dest, src, dim * sizeof(double));
}
