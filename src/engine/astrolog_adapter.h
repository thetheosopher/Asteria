#pragma once
#include <string>
#include <vector>
#include <mutex>
#include "domain/types.h"
#include "domain/location_resolution.h"
#include "domain/computed_chart.h"

namespace asteria::engine {

/// Thin C++ adapter around Astrolog's global state.
/// All calls are mutex-protected since Astrolog uses global variables.
/// This adapter translates between Asteria domain types and Astrolog internals.
class AstrologAdapter {
 public:
  /// Initialize the Astrolog engine. Must be called once before any computation.
  /// dataPath: directory containing astrolog.as and ephem/.
  /// Atlas and timezone lookup files are loaded separately by AtlasService.
  static bool initialize(const std::string& dataPath);

  /// Check if the engine has been initialized.
  static bool isInitialized();

  /// Get the Astrolog version string.
  static std::string engineVersion();

  struct ChartInput {
    int year = 2000;
    int month = 1;
    int day = 1;
    double timeHours = 12.0;   // 24-hour decimal (e.g. 14.5 = 2:30 PM)
    double dstHours = 0.0;     // Daylight saving offset
    double timezoneHours = 0.0; // Hours west of UTC (positive = west)
    double longitudeDeg = 0.0;  // Standard convention: positive = east
    double latitudeDeg = 0.0;   // Standard convention: positive = north
    int houseSystem = 0;        // 0 = Placidus (maps to Astrolog enum)
    bool sidereal = false;
  };

  struct ChartOutput {
    struct PlanetData {
      int objectIndex;
      std::string name;
      double longitude;    // 0-360 degrees
      double latitude;     // ecliptic declination
      double speed;        // retrograde if negative
      std::string sign;
      int signIndex;       // 1-12
      int house;           // 1-12
      bool retrograde;
    };
    struct HouseCuspData {
      int houseNumber;     // 1-12
      double longitude;    // 0-360 degrees
    };

    std::vector<PlanetData> planets;
    std::vector<HouseCuspData> houseCusps;
    std::string engineVersion;
    std::string engineMethod;
  };

  /// Compute a natal chart from the given input.
  static bool computeChart(const ChartInput& input, ChartOutput& output);

  /// Map a house system name string to Astrolog's numeric house system constant.
  static int mapHouseSystem(const std::string& name);

  /// Map a sign index (1-12) to its name.
  static std::string signName(int signIndex);

 private:
  static std::mutex s_mutex;
  static bool s_initialized;
  static std::string s_dataPath;

  /// Set Astrolog's global chart info from our input.
  static void setChartInfo(const ChartInput& input);

  /// Read computed results from Astrolog's global state.
  static void readResults(ChartOutput& output);
};

}  // namespace asteria::engine
