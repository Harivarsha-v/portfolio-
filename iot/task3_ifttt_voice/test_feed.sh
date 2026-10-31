# Writes ON to the feed. Replace the username and key with your own.
curl -X POST "https://io.adafruit.com/api/v2/YOUR_AIO_USERNAME/feeds/esp/data" \
     -H "X-AIO-Key: YOUR_AIO_KEY" \
     -H "Content-Type: application/json" \
     -d "{\"value\": \"ON\"}"

# A successful write returns the stored data point as JSON
# (id, value, feed_id, created_at). Send "OFF" the same way to switch the bulb off.
