#!/usr/bin/env python3
"""Optional browser check for actual offline A/B controls, audio and local links."""
import argparse
import functools
import http.server
import json
import pathlib
import threading
from playwright.sync_api import sync_playwright
p=argparse.ArgumentParser();p.add_argument('directory',type=pathlib.Path);p.add_argument('--report',type=pathlib.Path,required=True);a=p.parse_args()
root=a.directory.resolve()
import serve_evidence
server=serve_evidence.server(root,0)
threading.Thread(target=server.serve_forever,daemon=True).start()
errors=[]
with sync_playwright() as pw:
    browser=pw.chromium.launch(executable_path='/usr/bin/chromium',headless=True,args=['--autoplay-policy=no-user-gesture-required'])
    page=browser.new_page();page.on('pageerror',lambda e:errors.append(str(e)))
    page.on('response',lambda response:errors.append(str(response.status)+' '+response.url) if response.status>=400 else None)
    page.goto(f'http://127.0.0.1:{server.server_port}/index.html');page.wait_for_function("[...document.querySelectorAll('audio')].every(t=>t.readyState>=1)")
    group=page.locator('.ab').first
    group.locator('button[data-side=reference]').click();page.wait_for_timeout(60)
    group.locator('button[data-side=pause]').click()
    # Set a documented listening position and switch, then pause/seek/switch back.
    group.locator('audio[data-side=reference]').evaluate('(t)=>new Promise(resolve=>{t.addEventListener("seeked",resolve,{once:true});t.currentTime=.1})')
    group.locator('button[data-side=candidate]').click();page.wait_for_timeout(30)
    values=group.evaluate("g=>[...g.querySelectorAll('audio')].map(t=>({side:t.dataset.side,time:t.currentTime,paused:t.paused}))")
    assert values[0]['paused'] and not values[1]['paused'] and .1<=values[1]['time']<.3,values
    group.locator('button[data-side=pause]').click()
    group.locator('audio[data-side=candidate]').evaluate('(t)=>new Promise(resolve=>{t.addEventListener("seeked",resolve,{once:true});t.currentTime=.25})')
    group.locator('button[data-side=reference]').click();page.wait_for_timeout(30)
    reverse=group.evaluate("g=>[...g.querySelectorAll('audio')].map(t=>({side:t.dataset.side,time:t.currentTime,paused:t.paused}))")
    assert not reverse[0]['paused'] and reverse[1]['paused'] and .25<=reverse[0]['time']<.5,reverse
    for target in page.locator('a').evaluate_all('links=>links.map(a=>a.href)'):
        response=page.request.get(target);assert response.ok,target
    assert not errors,errors
    report={'status':'PASS','browser':'local Chromium headless','sections':page.locator('section').count(),
            'audio_metadata':'all loaded','local_links':'all HTTP 200','ab_switch':values,'paused_reverse_switch':reverse,'console_errors':errors}
    a.report.write_text(json.dumps(report,indent=2)+'\n');browser.close()
server.shutdown();print('PASS: A/B position, exclusivity, pause, audio and local report paths')
