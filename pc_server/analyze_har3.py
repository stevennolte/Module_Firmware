import json
from collections import Counter, defaultdict

# Load HAR file
with open(r'c:\Users\steve\Downloads\localhost3.har', 'r') as f:
    data = json.load(f)

entries = data['log']['entries']
print(f'Total requests: {len(entries)}\n')
print('='*80)
print('TIMING ANALYSIS - ALL REQUESTS:')
print('='*80)

# Group by endpoint
endpoint_times = defaultdict(list)
for e in entries:
    url = e['request']['url']
    # Extract endpoint
    if '/api/' in url:
        endpoint = url.split('/api/')[-1].split('?')[0]
    else:
        endpoint = 'static/page'
    
    time = e['time']
    endpoint_times[endpoint].append(time)

# Calculate stats for each endpoint
print(f"\n{'Endpoint':<50} {'Count':>6} {'Avg (ms)':>10} {'Min (ms)':>10} {'Max (ms)':>10}")
print('-'*86)
for endpoint in sorted(endpoint_times.keys()):
    times = endpoint_times[endpoint]
    count = len(times)
    avg = sum(times) / count
    min_time = min(times)
    max_time = max(times)
    print(f"{endpoint:<50} {count:>6} {avg:>10.1f} {min_time:>10.1f} {max_time:>10.1f}")

print('\n' + '='*80)
print('SLOW REQUESTS (> 2000ms):')
print('='*80)
slow_requests = []
for e in entries:
    if e['time'] > 2000:
        url = e['request']['url']
        time = e['time']
        timings = e['timings']
        slow_requests.append((time, url, timings))

slow_requests.sort(reverse=True)
for time, url, timings in slow_requests:
    print(f'\n{url}')
    print(f'  Total: {time:.1f}ms')
    print(f'  - Blocked: {timings["blocked"]:.1f}ms')
    print(f'  - DNS: {timings["dns"]:.1f}ms')
    print(f'  - Connect: {timings["connect"]:.1f}ms')
    print(f'  - Send: {timings["send"]:.1f}ms')
    print(f'  - Wait (server): {timings["wait"]:.1f}ms')
    print(f'  - Receive: {timings["receive"]:.1f}ms')

print('\n' + '='*80)
print('REQUESTS BY URL (most frequent):')
print('='*80)
urls = [e['request']['url'].split('?')[0] for e in entries]
for url, count in Counter(urls).most_common(10):
    print(f'{count:3d}x {url}')

print('\n' + '='*80)
print('GITHUB API CALLS (latest-firmware):')
print('='*80)
for e in entries:
    if 'latest-firmware' in e['request']['url']:
        url = e['request']['url']
        module = url.split('/')[-2]
        time = e['time']
        wait = e['timings']['wait']
        print(f'{module:<25} Total: {time:>7.1f}ms  Server wait: {wait:>7.1f}ms')
