/*
 ____  _____ _        _    
| __ )| ____| |      / \   
|  _ \|  _| | |     / _ \  
| |_) | |___| |___ / ___ \ 
|____/|_____|_____/_/   \_\

http://bela.io

*/

#include <Bela.h> //allows use of Bela device
#include <vector> //allows delay buffers to be created
#include "MonoFilePlayer.h" //allows playback of audio
#include <libraries/Gui/Gui.h>	//allows use of GUI
#include <libraries/GuiController/GuiController.h> //allows use of GUI sliders
#include <math.h>	//required for mathematical calculations sin, M_PI etc

std::string gFilename = "guitarloop.wav"; //loads my guitar loop
MonoFilePlayer gPlayer;	//used to play sound from file
Gui gGui;				//sets up GUI controls
GuiController gGuiController;	//set up GUI controllers

std::vector<float> gDelayBuffer;	//buffer required for LFO delay
std::vector<float> gDelayBuffer2;	//buffer required for 2nd delay
unsigned int gReadPointer[8] = {0};	//read pointer for LFO delay
unsigned int gWritePointer[8] = {0}; //write pointer for LFO delay
unsigned int gReadPointerB[8] = {0}; //where gReadPointerB = gReadPointer - 1, for interpolation
unsigned int gReadPointer2[8] = {0}; //read pointer for 2nd delay
unsigned int gWritePointer2[8] = {0}; //write pointer for 2nd delay
float out[8] = {0}; // LFO delay FX output
float out2[8] = {0}; //2nd delay FX output
float input[8] = {0}; //used to split input into 8 taps
float in2[8] = {0}; //for rewriting LFO delay output as second delay input
float output[8] = {0}; //LFO delay dry + FX output
float LeftOut[8] = {1, 0, 0, 1, 1, 0, 1, 0}; //panning scalar for L speaker
float LeftOutput[8] = {0};	//array out outputs for L channel for each pointer
float RightOut[8] = {0, 1, 1, 0, 0, 1, 0, 1}; //panning scalar for R speaker
float RightOutput[8] = {0}; //array out outputs for R channel for each pointer
float delaycontrol[8] = {23.6, 30, 38.1, 47.6, 300, 400, 341, 450}; //delay speed in ms per tap
float sweepwidth[8] = {3.5, 4, 4.2, 3.7, 3.5, 3.8, 4.7, 3.3}; //sweep width speed in ms per tap
float channelVolume[8] = {1, 1, 1, 1, 0.65, 0.65, 0.65, 0.65}; //scalar for each tap's output level
int i = 0; //initial value for sample counter
float feedback[8] = {0, 0, 0, 0, 0.45, 0.35, 0.43, 0.34}; //feedback scalar for each tap
float Mw[8] = {0};
float Mo[8] = {0};
float delay1[8] = {0}; //decimal value for delay in samples
int delay2[8] = {0};	//integer value for delay in samples
float interpolationNumber[8] = {0}; //value used for interpolation weighting for smooth output

bool setup(BelaContext *context, void *userData)
{
	if(!gPlayer.setup(gFilename)){
    	return false;} // Load the audio file
    			
    gDelayBuffer.resize(context->audioSampleRate);	//set first delay buffer to 1s long
    gDelayBuffer2.resize(context->audioSampleRate); //set second delay buffer to 1s long
    gGui.setup(context->projectName);	//initialise GUI
    gGuiController.setup(&gGui, "Parameters");	//label GUI. Below values are (name, default, min, max, increment)
    gGuiController.addSlider("Secondary delay time: ms", 375, 0, 500, 1);	//ms time of all secondary delays
    gGuiController.addSlider("Delay 1 feedback level", 0.25, 0, 0.95, 0);	//level of LFO delay
    gGuiController.addSlider("Delay 2 feedback level", 0.4, 0, 0.95, 0);	//level of 2nd delay w/feedback
    gGuiController.addSlider("LFO frequency", 2, 0, 6.0 , 0);				//sets LFO frequency
    gGuiController.addSlider("Dry level", 0.5, 0.0, 1.0 , 0);				//scalar for dry output level
    gGuiController.addSlider("FX level", 0.85, 0.0, 1.0 , 0);				//scalar for FX output level

	return true;
}

void render(BelaContext *context, void *userData)
{
	//the following six take values from the aforementioned GUI sliders
	int delayInSamples = gGuiController.getSliderValue(0)*context->audioSampleRate/1000; 
		//calulation for feedback delay
	float depth = gGuiController.getSliderValue(1);
	float delayScalar = gGuiController.getSliderValue(2);
	float fLFO = gGuiController.getSliderValue(3);
	float dryLevel = gGuiController.getSliderValue(4);
	float fxLevel = gGuiController.getSliderValue(5);
	
	
	
    for(unsigned int n = 0; n < context->audioFrames; n++) { //loop to run for each sample, once per second
        float sineCalc = sin(2*M_PI*fLFO*i/context->audioSampleRate);
        float in = gPlayer.process(); //takes input of next sample from audio file
        for (int a=0; a<8; a++){	//loop to calculate all 8 columns of each array
        input[a] = in;	//splits input signal into 8 taps
        Mo[a] = delaycontrol[a]*context->audioSampleRate/1000;	//converts delaycontrol from ms to samples
        Mw[a] = sweepwidth[a]*context->audioSampleRate/1000;	//converts sweepwidth from ms to samples
        delay1[a] = Mo[a] + (Mw[a]/2)*(1+sineCalc); //delay formula as decimal
        delay2[a] = Mo[a] + (Mw[a]/2)*(1+sineCalc);	//delay formula as integer
        interpolationNumber[a] = delay1[a] - delay2[a]; //decimal minus integer leaves scalar weighting for interpolation

        gReadPointer[a] = (gWritePointer[a] - delay2[a] + gDelayBuffer.size())%gDelayBuffer.size();
        	//gReadPointer reads 'delay2' samples behind gWritePointer. Modulo ensures sample number always positive
        
        gReadPointerB[a] = (gWritePointer[a] - delay2[a] - 1 + gDelayBuffer.size())%gDelayBuffer.size();
    		//as above but one further sample behind
       
        out[a] = ((1-interpolationNumber[a])*gDelayBuffer[gReadPointer[a]])+(interpolationNumber[a]*
        	gDelayBuffer[gReadPointerB[a]]);
        	//output is interpolated weighting of gReadPointer and gReadPointerB (=gReadPointer-1)
        	
        gDelayBuffer[gWritePointer[a]] = in; //writes current input to next column of delay buffer
       
        gWritePointer[a]++; //increments write pointer
        	if (gWritePointer[a] >= gDelayBuffer.size()) 
        		gWritePointer[a] = 0; //resets write pointer to zero after buffer length values			
        		
        output[a] = input[a]+out[a];	//output = input plus scaled version of delay
        
        //delay with Feedback algorithm below - very similar setup to the above
        gReadPointer2[a]=(gWritePointer2[a] - delayInSamples + gDelayBuffer2.size())%gDelayBuffer2.size();
        in2[a]=output[a];
        out2[a]=gDelayBuffer2[gReadPointer2[a]];
        gDelayBuffer2[gWritePointer2[a]]=in2[a]+delayScalar*out2[a]*feedback[a];
        
        gWritePointer2[a]++;
        if (gWritePointer2[a] >= gDelayBuffer2.size())
        	gWritePointer2[a]=0;
        gReadPointer2[a]++;
        if (gReadPointer2[a] >= gDelayBuffer2.size())
        	gReadPointer2[a]=0;
        
        LeftOutput[a]=LeftOut[a]*channelVolume[a]*(in2[a]+out2[a]); //left output array is current input and output,
        	//multiplied by pan settings and channel volume. Likewise with right output array
        RightOutput[a]=RightOut[a]*channelVolume[a]*(in2[a]+out2[a]);}
        
        float leftsum=0;		//initialise sums to add total of values in left and right arrays
        float rightsum = 0;
        for (int b=0; b<8; b++){
        leftsum += LeftOutput[b];
        rightsum += RightOutput[b];}	//for loop adds value at each array position to find total of values in array
        
        float FinalLeftOutput = leftsum*fxLevel*depth + in*dryLevel;	//add scalar values of L/R fx plus dry signal
        float FinalRightOutout = rightsum*fxLevel*depth + in*dryLevel;
        
    	audioWrite(context, n, 0, FinalLeftOutput);	//plays respective output for left and right speakers
    	audioWrite(context, n, 1, FinalRightOutout);
    	
    	i++; //increments sample number
        	if (i>= context->audioSampleRate/fLFO)
        		i=0; //resets sample number to zero after entire sine wave completed
    }
}

void cleanup(BelaContext *context, void *userData)
{

}

