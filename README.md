# CutDetective
Scene detection plugin for Flame and Smoke, which can also remove duplicate frames.  It writes an EDL which when linked back to the same clip has match cuts at the shot boundaries, and extra cuts to remove duplicate frames if desired. Quick demo:

[![CutDetective demo video](http://img.youtube.com/vi/ZKqxdYjC5Ns/3.jpg)](http://www.youtube.com/watch?v=ZKqxdYjC5Ns)

## Instructions
- Download from https://github.com/lcrs/CutDetective/releases/latest and unpack into /usr/discreet/sparks or similar
- In the timeline, add the Spark to the source clip.
- Enter the Spark editor and hit Analyse on the left.  You can analyse only a portion if you wish.
- When it's done, take a look at the Animation curves.  You can adjust the two threshold curves to suit difficult footage - only frames where the "Current difference" curve pokes out above the "Cut threshold" are considered to be cuts, and only frames where it's below the "Duplicate threshold" are considered dupes.
- Back in the Control page, enable or disable cut or duplicate frame detection, then save the EDL.
- Exit from the Spark editor, and remove the effect from the timeline.
- In the Conform tab, load the EDL that was just created, setting the framerate the same as in the Spark.
- Turn off all match criterea except Source Timecode, then select all shots and force them to link by right-clicking the source clip in the media panel and choosing Link.  Make sure the clip you're linking to has 0-based timecode, so doesn't start at 01:00:00:00 or anything like that.  It also needs to be a regular "source clip", it won't relink to something it thinks is a "Sequence" unfortunately, so you may need to match out from a record timeline first.  It's also best to keep the unlinked EDL out of the conform search location, to avoid the confusion of it attempting to link to itself.
- An alternative to relinking the EDL is to simply copy the source clip to a layer above the unlinked cuts, put the cursor on that new layer, then use Merge Tracks in the timeline gear menu, which copies the cuts from the bottom layer onto the top.  This way you don't need to worry about timecode, and you could even use the Spark on a gap effect above a series of clips, then copy the cuts to all of them at once.
- Check the cuts in the timeline!


## Both at once
If you need to both remove duplicates and also find cuts, it is possible to do both at once but the resulting timeline can look a little messy, because every removed frame adds an extra two cuts.  If possible, first save an EDL which just removes duplicates, conform that, and commit the resulting timeline to a single clip.  Then add the Spark again on this new clip, Analyse it again, and this time do only cut detection.
