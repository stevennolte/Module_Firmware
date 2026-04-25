import json
from collections import Counter

# Load HAR file
with open(r'c:\Users\steve\Downloads\localhost.har', 'r') as f:
    data = json.load(f)

entries = data['log']['entries']
print(f'Total requests: {len(entries)}')
print('\n' + '='*80)
print('REQUESTS BY URL:')
print('='*80)
urls = [e['request']['url'].split('?')[0] for e in entries]
for url, count in Counter(urls).most_common():
    print(f'{count:3d}x {url}')

print('\n' + '='*80)
print('STATUS CODES:')
print('='*80)
for e in entries:
    status = e['response']['status']
    method = e['request']['method']
    url = e['request']['url'].split('/')[-1] if '/' in e['request']['url'] else e['request']['url']
    print(f'{status} - {method:4s} {url[:60]}')

print('\n' + '='*80)
print('TIMING ANALYSIS (requests > 500ms):')
print('='*80)
for e in entries:
    time = e['time']
    if time > 500:
        url = e['request']['url']
        timings = e['timings']
        print(f'\n{url}')
        print(f'  Total: {time:.1f}ms')
        print(f'  - Blocked: {timings["blocked"]:.1f}ms')
        print(f'  - Connect: {timings["connect"]:.1f}ms')
        print(f'  - Send: {timings["send"]:.1f}ms')
        print(f'  - Wait: {timings["wait"]:.1f}ms')
        print(f'  - Receive: {timings["receive"]:.1f}ms')

print('\n' + '='*80)
print('POST REQUESTS:')
print('='*80)
for e in entries:
    if e['request']['method'] == 'POST':
        url = e['request']['url']
        status = e['response']['status']
        time = e['time']
        print(f'\n{url}')
        print(f'  Status: {status}')
        print(f'  Time: {time:.1f}ms')
        # Check for response body
        if 'content' in e['response'] and 'text' in e['response']['content']:
            text = e['response']['content']['text'][:200]
            print(f'  Response: {text}')

print('\n' + '='*80)
print('ERRORS OR WARNINGS:')
print('='*80)
error_found = False
for e in entries:
    status = e['response']['status']
    if status >= 400 or e['response'].get('_error'):
        error_found = True
        url = e['request']['url']
        print(f'{status} - {url}')
        if 'content' in e['response'] and 'text' in e['response']['content']:
            print(f'  Response: {e["response"]["content"]["text"][:200]}')
if not error_found:
    print('No errors found!')
