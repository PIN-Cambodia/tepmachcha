// Maxbotix Sonar
#include "tepmachcha.h"

#define SONAR_SAMPLES 11
#define SONAR_RETRIES 33

// insertion sort: https://en.wikipedia.org/wiki/Insertion_sort
void sort (int16_t *a, uint8_t n)
{
  for (uint8_t i = 1; i < n; i++)
  {
    // 'float' the current element up towards the beginning of array
    for (uint8_t j = i; j >= 1 && a[j] < a[j-1]; j--)
    {
      // swap (j, j-1)
      int16_t tmp = a[j];
      a[j] = a[j-1];
      a[j-1] = tmp;
    }
  }
}


// Calculate mode, or fall back to median of sorted samples
int16_t mode (int16_t *sample, uint8_t n)
{
    int16_t mode;
    uint8_t count = 1;
    uint8_t max_count = 1;

    for (int i = 1; i < n; i++)
    {
      if (sample[i] == sample[i - 1])
        count++;
      else
        count = 1;

      if (count > max_count)  // current sequence is the longest
      {
        max_count = count;
        mode = sample[i];
      }
      else if (count == max_count)  // no sequence (count == 1), or bimodal
      {
        mode = sample[(n/2)];       // use median
      }
    }
    return mode;
}

// Read Maxbotix MB7363 samples in free-run/filtered mode.
// Don't call this more than 6Hz due to min. 160ms sonar cycle time
// We reject up to SONAR_RETRIES invalid readings.
void sonarSamples(int16_t *sample)
{
    uint8_t retries = SONAR_RETRIES;
    uint8_t sampleCount = 0;

    digitalWrite (RANGE, HIGH);  // sonar on
    wait (1000);

    // wait for and discard first sample (160ms)
    //pulseIn (PING, HIGH);
    //wait (10);

    // read subsequent (filtered) samples into array
    // discard up to SONAR_RETRIES invalid readings
    while (sampleCount < SONAR_SAMPLES)
    {
      // 1 µs pulse = 1mm distance
      int16_t reading = pulseIn (PING, HIGH);

      // ~16 chars at 57600baud ~= 3ms delay
      Serial.print (F("Sample "));
      Serial.print (sampleCount);
      Serial.print (F(": "));
      Serial.print (reading);

      // After the PWM pulse, the sonar transmits the reading
      // on the serial pin, which takes ~10ms, we skip this.
      wait (15);

      Serial.println (F(" accept"));
      sample[sampleCount] = reading;
      sampleCount++;
    }

    digitalWrite (RANGE, LOW);   // sonar off
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
    sort (sample, SONAR_SAMPLES);

    // take the mode, or median
    distance = mode (sample, SONAR_SAMPLES);

    Serial.print (F("Surface distance from sensor is "));
    Serial.print (distance);
    Serial.println (F("mm."));

    return distance;
}
