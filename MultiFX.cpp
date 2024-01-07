/*
 ____  _____ _        _    
| __ )| ____| |      / \   
|  _ \|  _| | |     / _ \  
| |_) | |___| |___ / ___ \ 
|____/|_____|_____/_/   \_\

http://bela.io

---------------------------------------------------------MULTI EFFECTS - MATT WALFORD -----------------------------------------------------------------

The following project allows a user to apply a range of effects onto an uploaded piece of audio. I have recorded a guitar loop at 120bpm with slower
and faster individually plucked strings, plus a chord pattern, to demonstrate the effects with different settings. The effects are chorus, delay with
feedback, and a "width" effect. The chorus effect relies on the use of an oscillator to repeat a given sample very quickly, with the distance between
the original and repeated note varying slightly over time. The feedback with delay effect repeats an output indefinitely (though in reality it becomes
inaudible after several iterations if the 'scalar' is below 1) to simulate a decaying echo. The width delays the output to the right speaker, which
psychologically creates the effect of 'width' of sound. The parameters are controlled by the user with GUI sliders. The code allowing the reading of
the audio file, and playing the audio, is provided as open source code to be used with the Bela device, which is an external piece of hardware designed
for use in sound processing and electronic engineering. The code which takes it from the basic audio files to the processed version with effects is
written and designed by me, while the given code then converts my processed output to audio.

*/

#include <Bela.h>
#include <vector>
#include "MonoFilePlayer.h"
#include <libraries/Gui/Gui.h>
#include <libraries/GuiController/GuiController.h>
#include <math.h>

MonoFilePlayer gPlayer;
Gui gGui;
GuiController gGuiController;

std::string gFilename = "guitarloop.wav";

std::vector<float> chorusBuffer;
std::vector<float> delayBuffer;
std::vector<float> delayOffBuffer;
std::vector<float> outputBuffer;
std::vector<float> sineArray;
int i=0;
float chorusOut = 0;
int feedbackDelayInSamples = 0;
int delayReadPointer = 0;
int delayWritePointer = 0;
float delayOut = 0;

bool setup(BelaContext *context, void *userData)

{	if(!gPlayer.setup(gFilename)){
    return false;} // Load the audio file
    			
    chorusBuffer.resize(context->audioSampleRate);
    delayBuffer.resize(context->audioSampleRate);
    delayOffBuffer.resize(context->audioSampleRate);
    outputBuffer.resize(context->audioSampleRate);
    
    //calculate values for sin[n] between n=0 and n=sample rate - this saves computation time later on
    sineArray.resize(context->audioSampleRate);
    for(int a=0; a < context->audioSampleRate; a++){
		sineArray[a]=sin(2*M_PI*a/context->audioSampleRate);
	}
    
    gGui.setup(context->projectName);
    gGuiController.setup(&gGui, "Multi FX");
    
    gGuiController.addSlider("CHORUS: 0 off / 1 on", 1, 0, 1, 1);
    gGuiController.addSlider("CH Delay", 25, 20, 30, 0);
    gGuiController.addSlider("CH Rate", 2, 1, 3, 1);
    gGuiController.addSlider("CH Width", 3.5, 2, 5, 0);
    gGuiController.addSlider("CH Level", 5, 0, 10, 0);
    
    gGuiController.addSlider("DELAY: 0 off / 1 on", 0, 0, 1, 1);
    gGuiController.addSlider("DL time in ms", 375, 0, 1000, 1); //defaults for both note length and MS equate to dotted quaver at 120bpm
    gGuiController.addSlider("DL feedback", 0.5, 0, 0.95, 0);
    
    gGuiController.addSlider("Width", 1, 1, 50, 1);
   
	return true;
}

void render(BelaContext *context, void *userData)

{	//chorus parameters from GUI sliders
	int chorusOnOrOff = gGuiController.getSliderValue(0);
	float chorusDelay = gGuiController.getSliderValue(1);
	int chorusRate = gGuiController.getSliderValue(2);
	float chorusWidth = gGuiController.getSliderValue(3);
	float chorusLevel = gGuiController.getSliderValue(4);
	
	//delay parameters from GUI sliders
	int delayOnOrOff = gGuiController.getSliderValue(5);
	float delayInMS = gGuiController.getSliderValue(6);
	float delayFeedbackLevel = gGuiController.getSliderValue(7);
	
	//parameters for width (which adds a slight offset between L and R speakers)
	float width = gGuiController.getSliderValue(8);
	
	//create 'int' version of 'context->audioSampleRate' for modulo calculation
	float sampleRateFloat = context->audioSampleRate;
	int sampleRate = int(sampleRateFloat);
	
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float in = gPlayer.process();
		chorusBuffer[i] = in;
		
		/*The next 'IF' statement activates chorus settings, if chorus is switched on.
		A given input will be heard once, then repeated very close to the original sound.
		This delay length varies according to a low-frequency oscillator.
		The following code converts parameters given by delay time and oscillator depth, width and frequency
		from miliseconds to samples. As the frequency varies, the actual number of samples between the two
		outputs will usually not be an integer - the code incorporates 'linear interpolation' to provide an estimate
		for	the output where the sample number required is a decimal, with this estimate being a weighted average
		of the values of the sample either side of the decimal. The code uses the pre-calculated sinusoid values and
		a modulo function as part of the oscillator calculation - this significantly reduces computation time.
		*/
		
		if(chorusOnOrOff==1){
			int oscillatorPosition = (i*chorusRate) % sampleRate;
			float initialChorusDelayInSamples = (chorusDelay/1000)*context->audioSampleRate;
			float sweepwidthChorusDelayInSamples = (chorusWidth/1000)*context->audioSampleRate;
			float totalDelayInSamplesFloat = initialChorusDelayInSamples + (sweepwidthChorusDelayInSamples/2) * (1 + (sineArray[oscillatorPosition]));
			int totalDelayInSamplesInt = initialChorusDelayInSamples + (sweepwidthChorusDelayInSamples/2) * (1 + sineArray[oscillatorPosition]);
			float interpolationRatio = totalDelayInSamplesFloat - totalDelayInSamplesInt;
			int delayPointer1 = (i - totalDelayInSamplesInt + sampleRate) % sampleRate;
			int delayPointer2 = (delayPointer1 - 1 + sampleRate) % sampleRate;
			float chorusSum = (interpolationRatio * chorusBuffer[delayPointer2]) + ((1 - interpolationRatio) * chorusBuffer[delayPointer1]);
        	chorusOut = (in + chorusLevel*chorusSum/10);
			}
		
		else{
    		chorusOut=in;
			}
			
		/*The next IF/ELSE statement takes the output from the previous step, whether the chorus was activated or not, and feeds it into
		a delay with feedback (or not if it is switched off). It makes use of an additional buffer - this delay buffer is being written
		as the samples are read, adding the value at the current output once per sample. This means the output at a given sample is a sum 
		of the current output, plus a scaled version of a previous output - the 'feedbackDelayInSamples' calculation takes the GUI output
		for the delay time in MS and converts it to samples to the user can select a delay time appropriate to their chosen sound. My 
		guitar loop I am using for this project is at 120bpm, meaning a delay of 375ms gives the popular dotted quaver delay. */
		
		delayOffBuffer[i]=chorusOut;
		
		feedbackDelayInSamples = (delayInMS/1000) * context->audioSampleRate;
		
		delayReadPointer = (i - feedbackDelayInSamples + sampleRate) % sampleRate;
		
		if(delayOnOrOff==1){
			delayOut = delayBuffer[delayReadPointer];
		}	
		else{
			delayOut = delayOffBuffer[(i - feedbackDelayInSamples + sampleRate) % sampleRate]; //aligns playback with delayed version 
		}
		
		delayBuffer[i] = chorusOut + delayOut * delayFeedbackLevel;
		
		//add "width" by adding slight delay to R output
		
		outputBuffer[i]=delayOut;;
		int widthInSamples = (width/1000)*context->audioSampleRate;
		int widthPointer = (i - widthInSamples + sampleRate) % sampleRate;
		
		audioWrite(context, n, 0, outputBuffer[i]);
    	audioWrite(context, n, 1, outputBuffer[widthPointer]);
    		
		i++;
    	if (i>= context->audioSampleRate){
    		i=0;
    	}
    	
    } //end of 'for' loop
} // end of render

void cleanup(BelaContext *context, void *userData)
{

}
