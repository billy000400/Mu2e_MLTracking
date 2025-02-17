# @Author: Billy Li <billyli>
# @Date:   11-03-2021
# @Email:  li000400@umn.edu
# @Last modified by:   billyli
# @Last modified time: 11-25-2021



import sys
from pathlib import Path

import numpy as np
from matplotlib import pyplot as plt

util_dir = Path.cwd().parent.joinpath('Utility')
sys.path.insert(1, str(util_dir))
from Information import *
from HitGenerators import Event

track_dir = Path("../../tracks")
db_files = [track_dir.joinpath('val.db')]

# dist, db_files, hitNumCut=20):
gen = Event(db_files, 5)

for i in range(100):
    hit_all, track_all = gen.generate(mode='eval')

    for trkIdx, hitIdcPdgId in track_all.items():
        hitIdc = hitIdcPdgId[:-1]
        hits = [hit_all[hitIdx] for hitIdx in hitIdc]
        z = [vec[2] for vec in hits]
        t = [vec[3] for vec in hits]
        plt.scatter(z,t, label=str(trkIdx))
    plt.legend()
    plt.show()
