import json

# Load HAR file
with open(r'c:\Users\steve\Downloads\localhost2.har', 'r') as f:
    data = json.load(f)

entries = data['log']['entries']
print(f'Total requests: {len(entries)}\n')
print('='*80)
print('POST REQUESTS (UPDATES):')
print('='*80)
for e in entries:
    if e['request']['method'] == 'POST':
        url = e['request']['url']
        status = e['response']['status']
        time = e['time']
        module = url.split('/')[-2] if '/' in url else 'unknown'
        print(f'\n{module}: HTTP {status} - {time:.0f}ms')
        print(f'  URL: {url}')
        if 'content' in e['response'] and 'text' in e['response']['content']:
            text = e['response']['content']['text']
            print(f'  Response: {text[:500]}')

print('\n' + '='*80)
print('ERRORS (4xx, 5xx):')
print('='*80)
error_found = False
for e in entries:
    if e['response']['status'] >= 400:
        error_found = True
        url = e['request']['url']
        status = e['response']['status']
        method = e['request']['method']
        print(f'\n{method} {url}')
        print(f'  Status: {status}')
        if 'content' in e['response'] and 'text' in e['response']['content']:
            text = e['response']['content']['text']
            print(f'  Response:\n{text}')

if not error_found:
    print('No errors found!')

print('\n' + '='*80)
print('UPDATE REQUEST TIMING DETAILS:')
print('='*80)
for e in entries:
    if e['request']['method'] == 'POST' and 'update' in e['request']['url']:
        url = e['request']['url']
        timings = e['timings']
        print(f'\n{url}')
        print(f'  Total time: {e["time"]:.1f}ms')
        print(f'  - Blocked: {timings["blocked"]:.1f}ms')
        print(f'  - DNS: {timings["dns"]:.1f}ms')
        print(f'  - Connect: {timings["connect"]:.1f}ms')
        print(f'  - Send: {timings["send"]:.1f}ms')
        print(f'  - Wait (server processing): {timings["wait"]:.1f}ms')
        print(f'  - Receive: {timings["receive"]:.1f}ms')
