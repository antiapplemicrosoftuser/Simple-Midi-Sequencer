import mido
import os
print("Please input filename. ('.mid' is not necessary)")
filename = input()
if (filename.lower() == "task4_4" or filename.lower() == "baseprogram"):
    print("ERROR: wrong filename")
if not os.path.isdir(filename):
    os.mkdir(filename)
projectFilename = f"{filename}/{filename}.ino"
midFilename = f"{filename}.mid"
midi = mido.MidiFile(midFilename)
baseTick = 0
totalTick = 0
for i in range(len(midi.tracks[0])):
    totalTick += midi.tracks[0][i].time
totalTick = -(-totalTick // (midi.ticks_per_beat * 4)) * (midi.ticks_per_beat * 4)
noteSize = (totalTick * 8) // midi.ticks_per_beat # 32分を最小単位としている。
measureSize = totalTick // (midi.ticks_per_beat * 4)
measureSizeTxt = f"const int measureSize = {measureSize};\n"
notes = [([0] * 32) for _ in range(noteSize)]
writeNotes = [-1] * noteSize
t = 0
for i in range(1, len(midi.tracks[0]) - 1):
    baseTick += midi.tracks[0][i].time
    t = (baseTick * 8) // midi.ticks_per_beat
    newNote = midi.tracks[0][i].note - 48
    if midi.tracks[0][i].type == "note_on":
        if (newNote < 0 or newNote >= 32):
            print("ERROR: note out of range -> " + str(newNote))
            exit()
        notes[t][newNote] = 1
    elif midi.tracks[0][i].type == "note_off":
        k = t - 1
        while notes[k][newNote] != 1:
            notes[k][newNote] = 1
            k -= 1
            if k < 0:
                print("ERROR: wrong massage")
                exit()
    else:
        print("ERROR: wrong type")
        exit()
for i in range(noteSize):
    for j in range(32):
        if notes[i][j] == 1:
            writeNotes[i] = 31 - j
writeTxt = ""
notesTxt = f"int notes[noteSize] = {str(writeNotes).replace('[', '{').replace(']', '}')};\n"
with open("baseProgram.ino", 'r', encoding='utf-8') as basefile:
    with open(projectFilename, 'w', encoding='utf-8') as projectfile:
        for i in range(414):
            projectfile.write(basefile.readline())
            if i == 37:
                projectfile.write(measureSizeTxt)
            elif i == 41:
                projectfile.write(notesTxt)
print(f"{projectFilename} imported!")