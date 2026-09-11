#pragma once
#include <string>
#include <vector>
#include <utility>

enum ZoneType
{
	Normal,
	Timed,
	Sample
};

struct Zone
{
	std::string name; // WC_Golem_Tower_1
	std::string filename; // WizardCity-WC_Streets-WC_Golem_Tower-WC_Golem_Tower_1
	std::string rawPath; // WizardCity/WC_Streets/WC_Golem_Tower/WC_Golem_Tower_1

	Zone(std::string name, std::string filename, std::string rawPath = "")
		: name(std::move(name)), filename(std::move(filename)), rawPath(std::move(rawPath)) {
	}
};

struct AccessPass
{
	std::string key; // FreeToPlay, WC-1
	ZoneType type;
	std::vector<Zone> zones;

	AccessPass(std::string key, ZoneType type) : key(key), type(type), zones({}) {} 
};