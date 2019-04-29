# README #  

This repository contains a c version of the beat tracking algorithm and a lua tool for integration with Renoise.
The tool creates a gui plugin that can be used to generate beats and sync Renoise's tempo automation.  
Take a look at the instructions in "Renoise-Instructions.pdf"

# SETUP #  

Install the xrnx tool by double clicking on it.
Renoise should open and say that it was successfully installed.
Now you should be able to see this menu entry: Tools > CSC499 > Beat Extract Tool.
Click on it to open the plugin.  

You will need to build the plugin by clicking "Build Executable"
Note that gcc and make are required to do this. 
If there are any errors then contact me and I can send you a build for windows or linux.  

# USAGE #  

Load any audio clip in to the audio editor, preferably when with a strong percussive beat.
Click on the slice button to start inserting markers.
About four beat markers is usually sufficient for most samples.
Click on "Load Beats as Slices" to run the executable and fill out the remaining beats.
Check if these beats look reasonable. If not, hit undo (ctrl z) and tweak your inputs.
To see how the result sounds, click "Slices to BPM Automation" to update the BPM automation track.
Add a drum beat to the pattern editor to see how the beats match up.
Check out Renoise-Instructions.pdf for a visual step by step guide.  

Take a look at beattrack_demo.xrns.
This file has been configured to work with the tool and contains a few examples to get you started.
Try to make a copy of one of the example songs and see if you can produce a similar result using the tool.
