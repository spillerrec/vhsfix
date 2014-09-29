vhsfix
======

Attempts to fix several issues in old VHS/Betamax recordings, in order to make higher quality digital backups.
Assumes input to be lossless YUY2@720x576.

Fixes:
- Completely messed up button 8 lines in VHS recordings with decent success.
- Top/Bottom lines in interlaced frames being slightly offset of some reason. (Seen in both VHS and Betamax.)

Possible improvements:
- Stabilisation between frames
- noise reduction in chroma planes, using multi frame techniques
- Doing colour enhancement