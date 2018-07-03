
#include "Lib/Integrators.h"
#include "Lib/Universe.h"
#include <vector>
#include <cassert>
#include <algorithm>

UniverseState operator+(const UniverseState &lhs, const UniverseState &rhs) {
    assert(lhs.state.size() == rhs.state.size());
    UniverseState newState;
    newState.state.reserve(lhs.state.size());
    for(size_t i = 0; i < lhs.state.size(); ++i) {
        newState.state.push_back(lhs.state[i] + rhs.state[i]);
    }
    return newState;
}

UniverseState operator*(const UniverseState &lhs, double rhs) {
    UniverseState newState;
    newState.state.reserve(lhs.state.size());
    for(size_t i = 0; i < lhs.state.size(); ++i) {
        newState.state.push_back(lhs.state[i] * rhs);
    }
    return newState;
}

UniverseDifferentiator::UniverseDifferentiator(double _sizeX, double _sizeY, double _forceFactor, double _gravity) {
    sizeX = _sizeX;
    sizeY = _sizeY;
    forceFactor = _forceFactor;
    gravity = _gravity;
}

UniverseState UniverseDifferentiator::derivative(const UniverseState &state) const {
    std::vector<Vector2D> forces(particles.size());
    for(size_t i = 0; i < particles.size(); ++i) {
        for(size_t j = 0; j < i; ++j) {
            Vector2D f = particles[i].computeForce(particles[j], state.state[i], state.state[j]);
            forces[i] += f;
            forces[j] -= f;
        }

        forces[i].x += boundForce(-state.state[i].pos.x);
        forces[i].x -= boundForce(state.state[i].pos.x - sizeX);
        forces[i].y += boundForce(-state.state[i].pos.y);
        forces[i].y -= boundForce(state.state[i].pos.y - sizeY);

        forces[i].y += gravity * particles[i].getMass();
    }

    UniverseState der;
    der.state.reserve(particles.size());
    for(size_t i = 0; i < particles.size(); ++i) {
        der.state.push_back(particles[i].derivative(state.state[i], forces[i]));
    }
    return der;
}

double UniverseDifferentiator::boundForce(double overEdge) const {
    if(overEdge < 0) return 0;
    return forceFactor * overEdge * overEdge * overEdge * overEdge;
}

Universe::Universe(double sizeX, double sizeY, double forceFactor, double gravity): diff(sizeX, sizeY, forceFactor, gravity) {
}

void Universe::addParticle(const ParticleType &pType, const ParticleState &pState) {
    diff.particles.push_back(pType);
    state.state.push_back(pState);
}

void Universe::removeParticle(int index) {
    diff.particles.erase(diff.particles.begin() + index);
    state.state.erase(state.state.begin() + index);
}

void Universe::advance(double dT) {
    advanceRungeKutta4(state, diff, dT);
}

Vector2D Universe::clampInto(const Vector2D &pos) {
    double newX = std::min(std::max(pos.x, 0.), diff.sizeX);
    double newY = std::min(std::max(pos.y, 0.), diff.sizeY);
    return Vector2D(newX, newY);
}
