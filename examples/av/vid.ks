#!/usr/bin/env ks
""" vid.ks - read video


@author: Cade Brown <cade@kscript.org>

"""

import av

# Open a video
fp = av.open("./assets/vid/rabbitman.mp4")

# Get the best streams
vid = fp.best_video()
aud = fp.best_audio()

for frame in vid {
    print (frame.shape)
}

for chunk in aud {
    print (chunk.shape)
}

