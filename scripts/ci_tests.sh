set -e

pip install -r requirements.txt

for instrument in oscillo spectrum pid
do
    make NAME=$instrument server
done
