class PhaseIncrementer
{
public:
    double getNextSample(int c)
    {
        double output = currentAngle[c];
        currentAngle[c] += angleChange;
        if (currentAngle[c] > pix2){
            currentAngle[c] -= pix2;
        }
        return output;
    }
    
    void setFrequency (double freq)
    {
        frequency = freq;
        angleChange = frequency * 2.0f * M_PI / Fs;
    }
    
    void prepare (double sampleRate, int )
    {
        Fs = sampleRate;
        angleChange = frequency * 2.0f * M_PI / Fs;
    }
    
private:
    double pix2 = 2.0 * M_PI;
    double currentAngle[2] = {0.0};
    double angleChange = 0.0;
    double frequency = 1.0;
    double Fs = 44100.0;
};
