
#include <cmath>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <filesystem>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include "Display.h"
#include "Globals.h"

void CallbackHandler::mouseCallback(int event, int x, int y, int flags, void *userdata) {
    CallbackHandler &thisHandler = * (CallbackHandler *) userdata;
    if(x != -1 || y != -1) thisHandler.pos = Vector2D(x, y);

    switch(event) {
    case CV_EVENT_LBUTTONDOWN: thisHandler.leftDown = true; break;
    case CV_EVENT_LBUTTONUP: thisHandler.leftDown = false; break;
    case CV_EVENT_RBUTTONDOWN: thisHandler.rightDown = true; break;
    case CV_EVENT_RBUTTONUP: thisHandler.rightDown = false; break;
    case CV_EVENT_MOUSEHWHEEL:
        thisHandler.radius *= pow(1.2, cv::getMouseWheelDelta(flags));
        thisHandler.radius = std::max(thisHandler.radius, 10.);
        thisHandler.radius = std::min(thisHandler.radius, 200.);
        break;
    }
    thisHandler.sign = (int) thisHandler.leftDown - (int) thisHandler.rightDown;
}

void CallbackHandler::setActionFromKey(int key) {
    switch(key) {
        case 'h': action = MouseAction::heat; break;
        case 'p': action = MouseAction::push; break;
        case 'c': action = MouseAction::create; break;
        case 's': action = MouseAction::spray; break;
    }
}


void UniverseModifier::modify(Universe &universe, const CallbackHandler &handler, double dT, int type) {
    if(! handler.sign) return; // No action from user

    modifyExisting(universe, handler, dT);
    addNew(universe, handler, dT, type);
}

void UniverseModifier::modifyExisting(Universe &universe, const CallbackHandler &handler, double dT) {
    const double heatingSpeed = 0.1;
    const double pushingSpeed = 0.5, pullingSpeed = 0.2;
    const double removeSpeed = 0.5;

    auto it = universe.begin();
    while(it != universe.end()) {
        bool skipIncrement = false;
        auto &state = *it;
        if((state.pos - handler.pos).magnitude() < handler.radius) {
            switch(handler.action) {
            case MouseAction::heat:
                state.v *= 1. + handler.sign * heatingSpeed * dT;
                break;
            case MouseAction::push:
                if(handler.sign > 0) {
                    state.v += (state.pos - handler.pos) * (pushingSpeed * dT / handler.radius);
                } else if (handler.sign < 0) {
                    state.v -= (state.pos - handler.pos) * (pullingSpeed * dT / handler.radius);
                    state.v *= 1. - heatingSpeed * dT;
                }
                break;
            case MouseAction::create:
                if(handler.sign > 0) { // pull and slow particles
                    state.v -= (state.pos - handler.pos) * (pullingSpeed * dT / handler.radius);
                    state.v *= 1. - heatingSpeed * dT;
                }
            case MouseAction::spray:
                if(handler.sign == -1) {
                    double prob = removeSpeed * dT;
                    if(std::bernoulli_distribution(prob)(randomGenerator)) {
                        it = universe.erase(it);
                        skipIncrement = true;
                    }
                }
                break;
            }
        }

        if(! skipIncrement) ++it;
    }
}

void UniverseModifier::addNew(Universe &universe, const CallbackHandler &handler, double dT, int type) {
    if(handler.sign <= 0) return;

    const double creationRadiusCoef = 0.8;
    const double sprayParticleSpeedCoef = 0.08;
    const double newParticlesCoef = 0.04;

    auto phiDistr = std::uniform_real_distribution<>(0., 2 * M_PI);
    std::array<double, 2> x = {0, creationRadiusCoef * handler.radius};
    std::array<double, 2> y = {0, 1};
    auto rDistr = std::piecewise_linear_distribution<>(x.begin(), x.end(), y.begin());

    switch(handler.action) {
    case MouseAction::create: {
        int newParticles = newParticlesCoef * handler.radius + 1;
        for(int i = 0; i < newParticles; ++i) {
            double phi = phiDistr(randomGenerator);
            double r = rDistr(randomGenerator);
            auto pos = universe.clampInto(handler.pos + Vector2D(r * cos(phi), r * sin(phi)));
            auto state = ParticleState(pos);
            universe.addParticle(type, state);
        }
        break;
    }
    case MouseAction::spray: {
        double phi = phiDistr(randomGenerator);
        double velocity = sprayParticleSpeedCoef * handler.radius;
        auto state = ParticleState(handler.pos, Vector2D(velocity * cos(phi), velocity * sin(phi)));
        universe.addParticle(type, state);
        break;
    }
    default: break;
    }
}


Display::Display(size_t _sizeX, size_t _sizeY, const std::string &_caption, const std::string &recordingPath):
    sizeX(_sizeX), sizeY(_sizeY), caption(_caption) {
    cv::namedWindow(caption, cv::WINDOW_AUTOSIZE);
    cv::setMouseCallback(caption, CallbackHandler::mouseCallback, & handler);

    if(! recordingPath.empty()) {
        auto path = std::filesystem::path(recordingPath);
        std::filesystem::create_directories(path.parent_path());
        recorder.open(recordingPath, CV_FOURCC('M','J','P','G'), 60, cv::Size(sizeX, sizeY));
    }
}

const CallbackHandler & Display::update(Universe &universe) {
    auto img = drawParticles(universe);
    drawPointer(img);
    drawStats(img, universe);

    if(recorder.isOpened()) {
        recorder.write(img);

        auto now = std::chrono::system_clock::now();
        auto millisFromEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
        if(millisFromEpoch % 1000 < 500)
            drawText(img, "Recording...", cv::Point(30, 60));
    }

    cv::imshow(caption, img);
    handler.setActionFromKey(cv::waitKey(1));
    return handler;
}


void Display::drawPointer(cv::Mat &img) const {
    auto circleColor = cv::Scalar(255, 255, 255);
    if(handler.sign > 0) circleColor = cv::Scalar(0, 0, 255);
    if(handler.sign < 0) circleColor = cv::Scalar(255, 0, 0);
    cv::circle(img, cv::Point(handler.pos.x, handler.pos.y), handler.radius, circleColor, 1);

    std::string text = "";
    if(handler.action == MouseAction::heat) text = "Heat mode";
    if(handler.action == MouseAction::push) text = "Push mode";
    if(handler.action == MouseAction::create) text = "Create mode";
    if(handler.action == MouseAction::spray) text = "Spray mode";
    drawText(img, text, cv::Point(30, 120));
}

void Display::drawStats(cv::Mat &img, Universe &universe) const {
    auto [n, velocity, temp] = computeStats(universe);

    const int prec = 2;
    drawText(img, "n = " + std::to_string(n), cv::Point(30, 170));
    drawText(img, "velocity = " + to_string(velocity, prec), cv::Point(30, 200));
    drawText(img, "temp = " + to_string(temp, prec), cv::Point(30, 230));
}

void Display::drawText(cv::Mat &img, const std::string &text, const cv::Point &loc) const {
    cv::putText(img, text, loc, cv::FONT_HERSHEY_PLAIN, 2., textColor, 2);
}

std::tuple<int, double, double> Display::computeStats(Universe &universe) const {
    int n = 0;
    double mass = 0;
    Vector2D momentum;

    // Compute velocity
    for(auto it = universe.begin(); it != universe.end(); ++it) {
        double pMass = it->type->getMass();
        Vector2D &pPos = it->pos, &pV = it->v;
        if((pPos - handler.pos).magnitude2() < handler.radius * handler.radius) {
            ++n;
            mass += pMass;
            momentum += pV * pMass;
        }
    }
    Vector2D velocity = momentum / mass;

    // Compute kinetic energy relative to average speed
    double energy = 0;
    for(auto it = universe.begin(); it != universe.end(); ++it) {
        double pMass = it->type->getMass();
        Vector2D &pPos = it->pos, &pV = it->v;
        if((pPos - handler.pos).magnitude2() < handler.radius * handler.radius)
            energy += (pV - velocity).magnitude2() * pMass / 2;
    }
    double temp = energy / n; // E = kT * (degrees of freedom = 2) / 2, k == 1 (natural units)
    return { n, velocity.magnitude(), temp };
}

cv::Mat Display::drawParticles(Universe &universe) const {
    auto img = cv::Mat(cv::Size(sizeX, sizeY), CV_8UC3, cv::Scalar(0, 0, 0));
    for(auto it = universe.begin(); it != universe.end(); ++it) {
        double radius = 0.6 * it->type->getRadius();
        cv::circle(img, cv::Point2i(it->pos.x, it->pos.y), radius, it->type->getColor(), -1);
    }
    return img;
}


std::string to_string(double x, int precision) {
    std::stringstream ss;
    ss << std::setprecision(precision) << std::fixed << x;
    return ss.str();
}
