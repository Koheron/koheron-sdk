set -e

pip install -r requirements.txt

python scripts/make.py --tests

for instrument in oscillo spectrum pid
do
    make NAME=$instrument server
done
