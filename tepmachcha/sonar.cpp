// Maxbotix Sonar
#include "tepmachcha.h"

#define SONAR_SAMPLES 11

//Sorting function

// sort function (Author: Bill Gentles, Nov. 12, 2010)
void isort(int *a, int n) {
  //  *a is an array pointer function

  for (int i = 1; i < n; ++i)
  {
    int j = a[i];
    int k;
    for (k = i - 1; (k >= 0) && (j < a[k]); k--)
    {
      a[k + 1] = a[k];
    }
    a[k + 1] = j;
  }
}

//Mode function, returning the mode or median.
int mode(int *x, int n) {
  int i = 0;
  int count = 0;
  int maxCount = 0;
  int mode = 0;
  int bimodal;
  int prevCount = 0;

  while (i < (n - 1)) {
    prevCount = count;
    count = 0;
    
    while (x[i] == x[i + 1]) {
      count++;
      i++;
    }

    if (count > prevCount & count > maxCount) {
      mode = x[i];
      maxCount = count;
      bimodal = 0;
    }

    if (count == 0) {
      i++;
    }

    if (count == maxCount) { //If the dataset has 2 or more modes.
      bimodal = 1;
    }
    if (mode == 0 || bimodal == 1) { //Return the median if there is no mode.
      mode = x[(n / 2)];
    }
    return mode;
  }
}

// Read Maxbotix MB7363 samples in free-run/filtered mode.
// Don't call this more than 6Hz due to min. 160ms sonar cycle time
void sonarSamples(int16_t *sample)
{
    uint8_t sampleCount = 0;

    digitalWrite (SONAR_PWR, HIGH);  // sonar on
    wait (160);

    // read subsequent (filtered) samples into array
    while (sampleCount < SONAR_SAMPLES)
    {
      // 1 µs pulse = 1mm distance
      int16_t reading = pulseIn (SONAR_PW, HIGH);

      Serial.print (F("Sample "));
      Serial.print (sampleCount);
      Serial.print (F(": "));
      Serial.print (reading);

      // After the PWM pulse, the sonar transmits the reading
      // on the serial pin, which takes ~160ms, we skip this.
      wait (160);

      Serial.println (F(" accept"));
      sample[sampleCount] = reading;
      sampleCount++;
    }

    digitalWrite (SONAR_PWR, LOW);   // sonar off
}

// Take a set of readings and process them into a single estimate
// We retry 3 times attempting to get a valid result
int16_t sonarRead (void)
{
    int16_t sample[SONAR_SAMPLES];
    int16_t distance;
    
    // read from sensor into sample array
    sonarSamples (sample);

    // TODO: Leo says this doesnt work
    // sort the samples
    isort (sample, SONAR_SAMPLES);

    // take the mode, or median
    distance = mode (sample, SONAR_SAMPLES);

    Serial.print (F("Surface distance from sensor between "));
    Serial.print (sample[0]);
    Serial.print (F(" and "));
    Serial.print (sample[SONAR_SAMPLES-1]);
    Serial.print (F("mm. "));

    Serial.print (F("Sending "));
    Serial.println(distance);

    return distance;
}
