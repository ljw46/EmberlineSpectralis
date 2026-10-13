# Downloads the AKWF families used by build.py (run it inside a scratch folder; it creates ./src)
import urllib.request, os, sys
from concurrent.futures import ThreadPoolExecutor
B="https://raw.githubusercontent.com/KristofferKarlAxelEkstrand/AKWF-FREE/main/AKWF"
fam={"hvoice":104,"flute":17,"clarinett":25,"oboe":13,"cello":19,"violin":14,"altosax":26,"aguitar":38,"eguitar":22,"piano":30,"theremin":26,"birds":14,
     "epiano":73,"eorgan":154,"clavinet":33,"ebass":70,"dbass":69,"bw_blended":73,"bw_sawrounded":52,"bw_squrounded":52,"overtone":44,"hdrawn":50,
     "fmsynth":122,"oscchip":158,"vgame":137,"distorted":45,"granular":44,"snippets":47,"sinharm":16,"c604":32,"linear":85,"stringbox":6,"pluckalgo":9,"raw":36}
jobs=[(f,i) for f,n in fam.items() for i in range(1,n+1)]
def get(j):
    f,i=j; name=f"AKWF_{f}_{i:04d}.wav"; d=f"src/{f}"; os.makedirs(d,exist_ok=True); p=f"{d}/{name}"
    if os.path.exists(p) and os.path.getsize(p)>100: return 1
    try:
        urllib.request.urlretrieve(f"{B}/AKWF_{f}/{name}", p); return 1
    except Exception as e:
        return 0
with ThreadPoolExecutor(16) as ex: ok=sum(ex.map(get,jobs))
print(f"downloaded {ok}/{len(jobs)} files")
