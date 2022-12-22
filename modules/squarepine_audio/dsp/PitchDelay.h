//
//  PitchDelay.hpp
//
//  Created by Eric Tarr on 2/6/20.
//  Copyright © 2020 Eric Tarr. All rights reserved.
//

class PitchDelay {
    
public:
    
    PitchDelay(int phaseChoice);
    
    float processSample(float x,int channel,float & angle);

    void setFs(float Fs);
    
    void setPitch(float semitone);

    
private:
    
    float Fs = 48000.f;
    int phaseChoice;
    
    const float MAX_DELAY_SEC = .02f;
    float MAX_DELAY_SAMPLES = MAX_DELAY_SEC * Fs;
    float delay[2];// = {5.f}; // in samples
    
    const int MAX_BUFFER_SIZE = 96000;
    float delayBuffer[96000][2] = {{0.0f}};
    int index[2] = {0};
    
    float semitone = 0.f;
    float tr = 1.f; // momentary trasposition
    float delta = 0.f; // rate of change for delay (samples)
    
};

