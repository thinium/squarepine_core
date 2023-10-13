class PhaseIncrementer
{
public:
    float getNextSample(int c)
    {
        float output = currentAngle[c];
        currentAngle[c] += angleChange;
        if (currentAngle[c] > pix2){
            currentAngle[c] -= pix2;
        }
        return output;
    }
    
    void setFrequency (float freq)
    {
        frequency = freq;
        angleChange = frequency * 2.0f * f_PI / Fs;
    }
    
    void prepare (double sampleRate, int )
    {
        Fs = static_cast<float> (sampleRate);
        angleChange = frequency * 2.0f * f_PI / Fs;
    }
    
    void setCurrentAngle (float angleInRadians, int channel)
    {
        currentAngle[channel] = angleInRadians;
    }
    
private:
    
    float f_PI = static_cast<float> (M_PI);
    
    float pix2 = 2.0f * f_PI;
    float currentAngle[2] = {0.0f};
    float angleChange = 0.0f;
    float frequency = 1.0f;
    float Fs = 44100.0f;
};
