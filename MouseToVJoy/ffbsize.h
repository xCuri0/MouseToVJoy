#pragma once
#ifndef FFBSIZE_H
#define FFBSIZE_H
#include <string>
using namespace std;
struct FFBSIZE {
private:
	//Forcefeedback data
	string _effectTypeStr = "";
	int _magnitudeVar = 0;
	int _directionVar = 0;
	int _offsetVar = 0;
	int _periodVar = 0;
	int _durationVar = 0;
public:
	//fuctions that returns forcefeedback data
	string getEffectType() { return _effectTypeStr; }
	int getMagnitude() { return _magnitudeVar; }
	int getDirection() { return _directionVar; }
	int getOffset() { return _offsetVar; }
	int getPeriod() { return _periodVar; }
	int getDuration() { return _durationVar; }
	void setEffectType(string type) { if (type != "NULL") { _effectTypeStr = type; } }
	void setMagnitude(int size) { if (size != NULL) { _magnitudeVar = size; } }
	void setDirection(int size) { if (size != NULL) { _directionVar = size; } }
	void setOffset(int size) { if (size != NULL) { _offsetVar = size; } }
	void setPeriod(int size) { if (size != NULL) { _periodVar = size; } }
	void setDuration(int size) { if (size != NULL) { _durationVar = size; } }
};
#endif