/*  Dusk2Dawn.h
 *  Get time of sunrise and sunset.
 *  Created by DM Kishi <dm.kishi@gmail.com> on 2017-02-01.
 *  <https://github.com/dmkishi/Dusk2Dawn>
 */

#ifndef Dusk2Dawn_h
#define Dusk2Dawn_h

  #include <math.h>
  #include <stdbool.h> 

  struct Dusk2Dawn{
    float latitude; 
    float longitude; 
    float timezone;
  };
  int sunrise(struct Dusk2Dawn d2d, int y, int m, int d, bool isDST);
  int sunset(struct Dusk2Dawn d2d,int y, int m, int d, bool isDST);

  /******************************************************************************/
  /*                                  PRIVATE                                   */
  /******************************************************************************/
  int sunriseSet(struct Dusk2Dawn d2d, bool isRise, int y, int m, int d, bool isDST);
  float sunriseSetUTC(bool isRise, float jday, float latitude, float longitude);
  /* ---------------------------- EQUATION OF TIME ---------------------------- */
  float equationOfTime(float t);
  float meanObliquityOfEcliptic(float t);
  float eccentricityEarthOrbit(float t);
  /* --------------------------- SOLAR DECLINATION ---------------------------- */
  float sunDeclination(float t);
  float sunApparentLong(float t);
  float sunTrueLong(float t);
  float sunEqOfCenter(float t);
  /* ------------------------------- HOUR ANGLE ------------------------------- */
  float hourAngleSunrise(float lat, float solarDec);
  /* ---------------------------- SHARED FUNCTIONS ---------------------------- */
  float obliquityCorrection(float t);
  float geomMeanLongSun(float t);
  float geomMeanAnomalySun(float t);
  /* --------------------------- UTILITY FUNCTIONS ---------------------------- */
  float jDay(int year, int month, int day);
  float fractionOfCentury(float jd);
  float radToDeg(float rad);
  float degToRad(float deg);


#endif