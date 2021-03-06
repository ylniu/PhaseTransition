#ifndef __PARTICLE_TYPE_H__
#define __PARTICLE_TYPE_H__

#include "Lib/Vector2.h"
#include <SDL2/SDL.h>
#include <string>

class ParticleType;

struct ParticleState {
    const ParticleType *type = nullptr;
    Vector2D pos, v;

    ParticleState();
    ParticleState(const Vector2D &_pos);
    ParticleState(const Vector2D &_pos, const Vector2D &_v);

    ParticleState & operator+=(const ParticleState & rhs);
    ParticleState & operator*=(double rhs);

    Vector2D computeForce(const ParticleState &rhs) const;
};

ParticleState operator+(const ParticleState & lhs, const ParticleState & rhs);
ParticleState operator*(const ParticleState & lhs, double rhs);

class ParticleType {
public:
    ParticleType(double _mass, double _radius, double _exclusionConstant, double _dipoleMoment, double _range);
    ParticleType(const std::string &_name, const std::string &spritePath,
        double _mass, double _radius, double _exclusionConstant, double _dipoleMoment, double _range);
    Vector2D computeForce(const ParticleType &other, const ParticleState &myState, const ParticleState &otherState) const;

    inline const std::string& getName() const { return name; }
    inline const SDL_Surface *getSpriteSurface() const { return spriteSurface; }
    inline double getRange() const { return range; }
    inline double getMass() const { return mass; }
    inline double getRadius() const { return radius; }

private:
    double computeForceComponent(double d) const;
    double computeForceFactor(double totalRadius, double minRange, double dist) const;
    double superSmoothZeroToOne(double x) const;

    std::string name;
    double mass, radius, exclusionConstant, dipoleMoment, range;
    SDL_Surface *spriteSurface = nullptr;
};

#endif
