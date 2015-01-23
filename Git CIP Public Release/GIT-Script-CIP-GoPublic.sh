#!/bin/bash

# This script assumes that we have already created a public repository called CIP (that will have to be replaced by just CIP)

# TODO: create repository and user permissions

# Go to private repository
cd ChestImagingPlatformPrivate

echo Updating CIP-Private...

# Make sure you are in master branch
git checkout master
# Get the last version
git pull

echo Copying CIP-Private to CIP develop branch...
# Push the CIPP master branch to the new repo in the "develop" branch (the new branch is created)
git push git@github.com:acil-bwh/ChestImagingPlatform.git master:develop

cd ..
echo Downloading CIP develop branch...
# Get the develop branch to a new folder
git clone git@github.com:acil-bwh/ChestImagingPlatform.git --branch develop

cd ChestImagingPlatform

# Create the release branch (local) from develop branch and switch to it
echo Creating release branch local...
git checkout -b release 

# Remove the folders that are not going to be part of the first release
echo Removing files that are not going to be part of the release...
rm -R "CommandLineTools/EvaluateLungLobeSegmentationResults"
#rm -R "CommandLineTools/FitFissureSurfaceModelToParticleData"
#rm -R "CommandLineTools/GenerateInteractiveLobeSegmentationFissureMask"
#rm -R "CommandLineTools/GenerateSliceBasedConvexHull"
rm -R "CommandLineTools/GenerateStatisticsForAirwayGenerationLabeling"
rm -R "CommandLineTools/LabelAirwayParticlesByGeneration"
rm -R "CommandLineTools/RemoveChestTypeFromLabelMapUsingParticles"
rm -R "CommandLineTools/RegisterLungAtlas"
rm -R "CommandLineTools/GenerateAtlasConvexHull"
rm -R "CommandLineTools/ComputeFissureFeatureVectors"
rm -R "CommandLineTools/EnhanceFissuresInImage"
rm -R "CommandLineTools/ReadVidaWriteCIP"
rm -R "CommandLineTools/GenerateDistanceMapFromLabelMap"

rm -R "InteractiveTools"

rm "cip_python/phenotypes/bronchiectasis.py" 
rm "cip_python/phenotypes/diaphragm_thickness.py"
rm "cip_python/phenotypes/gas_trapping.py"
rm "cip_python/phenotypes/vasculature.py"
rm -R "cip_python/segmentation"


# Override the Cmake files removing references to the removed elements
cp ../CIP-Reduced/CMakeLists.txt ./CommandLineTools  	# Removed CLIs
cp ../CIP-Reduced/CIP.cmake .							# InteractiveTools
cp ../CIP-Reduced/Documentation.h ./Documentation		# InteractiveTools 


# Commit the changes locally
echo Commit changes to release branch...
git commit -am "First release with tested tools"


# Push the changes to the remote repository (release branch)
echo Pushing changes to remote CIP repository...
git push -u origin release

## MASTER BRANCH

# Create a master branch as a copy of the release one
#echo Creating master branch from release branch...
#git push git@github.com:acil-bwh/ChestImagingPlatform.git release:master

# Go to the master branch and tag it
# git pull	# Bring the master branch
# git checkout master		# Switch to it
# echo Tagging master branch v1.0
# git tag -a v1.0 -m 'Welcome to CIP!'
# git push origin v1.0

#echo WELCOME TO CIP 1.0!! Remember that you are now in master branch...




