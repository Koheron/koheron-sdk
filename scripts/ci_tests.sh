set -e

pip install -r requirements.txt

python scripts/make.py --test

for instrument in oscillo spectrum pid blink
do
    make NAME=$instrument server
done
