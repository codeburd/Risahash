There's not much to this. Feed it a wav, get a 64-bit hash. Then compare those hashes using Hamming distance to find files which contain very similar audio. It's good for deduplicating music collections.

It's the RIdiculously Simple Audio Hash. And it really is simple.
- Chop audio into 65 sections (and thus 64 transitions between them - 65 posts for 64 fences.)
- At each transition determine if the volume went up or down.
- Turn that into a 64-bit output. That's all there is to it.

  Despite the extreme simplicity, it actually works very well - not only finding different encodings of the same song, but sometimes even the same song covered by different artists.

  Resistant to random noise and encoding noise, and to slightly different mixes. But useless for detecting shifted audio, even if it's just cutting out a few seconds of slience at the end of a track.
