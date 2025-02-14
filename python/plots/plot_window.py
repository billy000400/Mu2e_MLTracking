import sys
from pathlib import Path

import numpy as np
from matplotlib import pyplot as plt

util_dir = Path.cwd().parent.joinpath('Utility')
sys.path.insert(1, str(util_dir))
from Information import *
from HitGenerators import Event_V2 as Event

track_dir = Path("../../tracks")
db_files = [track_dir.joinpath('train_CeEndpoint-mix-fromCSV_1.db')]

# dist, db_files, hitNumCut=20):

# find the longest track
def findLongestTrk(tracks):
    idx = None
    lmax = 0
    for trkIdx, hits in tracks.items():
        l = len(hits)
        if l > lmax:
            idx=trkIdx
    return trkIdx

# find the smallest t among tracks
def find_tmin(tracks):
    ts = []
    for trkIdx, hits in tracks.items():
        for hit in hits:
            ts.append(hit[3])
    return min(ts)

windowNum = 100
trackNums = []

gen = Event(db_files, hitNumCut=13, totNum=windowNum)
for idx in range(windowNum):
    sys.stdout.write(t_info(f'Parsing windows {idx+1}/{windowNum}', special='\r'))
    if idx+1 == windowNum:
        sys.stdout.write('\n')
    sys.stdout.flush()
    hit_all, track_all = gen.generate(mode='eval')

    while len(hit_all)!=0:
        # find hits in first 100 ns
        ts = [hit[3] for hitId, hit in hit_all.items()]
        tmin = min(ts)
        hitIds_selected = []
        for hitId, hit in hit_all.items():
            t = hit[3]
            if t-tmin > 100:
                continue
            else:
                hitIds_selected.append(hitId)

        # find which track they belong
        tracks_selected = {}
        for trkIdx, hitIdsPdgId in track_all.items():
            hitIds = hitIdsPdgId[0:-1]
            hitsInWindow = []
            for hitId in hitIds:
                if hitId in hitIds_selected:
                    hitsInWindow.append(hit_all[hitId])
            if len(hitsInWindow)!=0:
                tracks_selected[trkIdx] = hitsInWindow

        # plot them
        fig = plt.figure(figsize=(10, 10))
        ax = fig.add_subplot(111, projection='3d')

        for trkIdx, hitsInWindow in tracks_selected.items():
            xs = [hit[0] for hit in hitsInWindow]
            ys = [hit[1] for hit in hitsInWindow]
            zs = [hit[2] for hit in hitsInWindow]
            ax.scatter(xs, zs, ys, alpha=1, label=trkIdx)

        ax.set(xlim=[-810, 810], ylim=[-1600, 1600], zlim=[-810, 810])
        ax.set(title=f'hits for t in [{tmin}, {tmin+100}]')

        ax.legend()
        plt.show()
        plt.close()

        # find which track has most hits in 100 ns
        hitNumMax = 0
        longestTrkIdx = None
        for trkIdx, hitsInWindow in tracks_selected.items():
            if len(hitsInWindow)>hitNumMax:
                longestTrkIdx = trkIdx
                hitNumMax = len(hitsInWindow)

        # delete that track's hits
        hitIds_deleting = track_all[longestTrkIdx][0:-1]
        for hitId in hitIds_deleting:
            del hit_all[hitId]
