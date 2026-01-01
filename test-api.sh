#!/bin/bash

# SmartClock API Test Script
# Usage: ./test-api.sh [hostname or IP]

HOST=${1:-smartclock.local}
BASE_URL="http://$HOST"

echo "Testing SmartClock API at $BASE_URL"
echo "======================================"

echo -e "\n1. Get device status:"
curl -s "$BASE_URL/app.json" | jq .

echo -e "\n2. Get storage info:"
curl -s "$BASE_URL/space.json" | jq .

echo -e "\n3. Get brightness:"
curl -s "$BASE_URL/brt.json" | jq .

echo -e "\n4. Set brightness to 50%:"
curl -s "$BASE_URL/set?brt=50"

echo -e "\n\n5. Set theme to 3 (custom image):"
curl -s "$BASE_URL/set?theme=3"

echo -e "\n\n6. Send live update:"
curl -s -X POST "$BASE_URL/api/update" \
  -H "Content-Type: application/json" \
  -d '{
    "line1": "12:45",
    "line2": "2500W",
    "lineMedia": "Now playing: Test Song",
    "line3": "25°C",
    "line4": "60%",
    "line5": "720W",
    "line6": "5.2kW",
    "bar": 0.5
  }'

echo -e "\n\n7. Upload test image (if test.jpg exists):"
if [ -f test.jpg ]; then
  curl -s -F "file=@test.jpg" "$BASE_URL/doUpload?dir=/image/"
  echo -e "\n\n8. Display uploaded image:"
  curl -s "$BASE_URL/set?img=/image/test.jpg"
else
  echo "test.jpg not found - skipping upload test"
fi

echo -e "\n\nTests complete!"
