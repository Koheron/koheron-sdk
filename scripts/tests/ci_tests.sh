set -e

pip install -r requirements.txt

python scripts/make.py --test

for instrument in blink oscillo spectrum
do
    make NAME=$instrument server
done
