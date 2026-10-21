'use strict';

const LOGIN = 1, LOGIN_ACK = 2, LOGINOUT = 3, ONE_CHAT = 6, ADD_FRIEND = 7;
const CREATE_GROUP = 8, ADD_GROUP = 9, GROUP_CHAT = 10;
const OFFLINE_ACK = 11, LOAD_HISTORY = 12, LOAD_HISTORY_ACK = 13;
const DEL_FRIEND = 14, QUIT_GROUP = 15;
const LOAD_GROUP_HISTORY = 16, LOAD_GROUP_HISTORY_ACK = 17;

const COLORS = ['#7c6aff', '#ff6aac', '#3ddc84', '#ffbe00', '#ff8c5a', '#4dabf7', '#845ef7'];

let ws = null;
let me = null;
let friends = [];
let groups = [];
let activeFriendId = null;
let activeGroupId = null;
let modalAction = null;
let toastTimer = null;
const chatStore = {};

const $ = (id) => document.getElementById(id);
const chatKey = (type, id) => (type === 'group' ? 'g:' : 'f:') + id;

function avatarUrl(id) {
  return `https://api.dicebear.com/7.x/thumbs/svg?seed=${id}`;
}
function groupAvatarUrl(id) {
  return `https://api.dicebear.com/7.x/shapes/svg?seed=g${id}`;
}
function colorOf(id) {
  return COLORS[Math.abs(Number(id)) % COLORS.length];
}
function resolveName(userid) {
  const id = Number(userid);
  if (me && Number(me.id) === id) return me.name || `#${id}`;
  const f = friends.find((x) => Number(x.id) === id);
  if (f) return f.name || `#${id}`;
  for (const g of groups) {
    const u = (g.users || []).find((x) => Number(x.id) === id);
    if (u) return u.name || `#${id}`;
  }
  return `#${id}`;
}
function sendLogoutBeacon() {
  if (!ws || !me || ws.readyState !== 1) return;
  try {
    ws.send(JSON.stringify({ msgid: LOGINOUT, id: me.id }));
  } catch (e) {}
}
function wsUrl() {
  const host = location.hostname || '127.0.0.1';
  const proto = location.protocol === 'https:' ? 'wss' : 'ws';
  if (location.port === '8090' || location.port === '80' || location.port === '') {
    const port = location.port ? `:${location.port}` : '';
    return `${proto}://${host}${port}/ws`;
  }
  if (location.port === '8088') return `${proto}://${host}:8089/`;
  if (location.port === '8188') return `${proto}://${host}:8189/`;
  return `${proto}://${host}:8089/`;
}
function parseItem(raw) {
  if (raw == null) return null;
  if (typeof raw === 'object') return raw;
  try { return JSON.parse(raw); } catch (e) { return null; }
}
function nowLabel(t) {
  if (t) {
    const d = new Date(String(t).replace(' ', 'T'));
    if (!isNaN(d.getTime())) return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  }
  return new Date().toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
}
function toast(msg) {
  const el = $('toast');
  el.textContent = msg;
  el.classList.remove('hidden');
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => el.classList.add('hidden'), 2400);
}

function closeMenus() {
  $('composeMenu').classList.add('hidden');
  $('chatMoreMenu').classList.add('hidden');
}

function setLoggedIn(ok) {
  $('loginOverlay').classList.toggle('hidden', ok);
  if (ok && me) {
    $('myName').textContent = me.name;
    $('myStatus').textContent = `online #${me.id}`;
    const av = $('myAvatar');
    av.style.background = colorOf(me.id);
    av.style.backgroundImage = `url(${me.avatar || avatarUrl(me.id)})`;
    av.style.backgroundSize = 'cover';
    $('myInitial').style.display = 'none';
  }
}

function applyPresence(userid, online) {
  const id = Number(userid);
  friends = friends.map((f) => Number(f.id) === id ? { ...f, state: online ? 'online' : 'offline' } : f);
  groups = groups.map((g) => {
    if (!Array.isArray(g.users)) return g;
    return {
      ...g,
      users: g.users.map((u) => Number(u.id) === id ? { ...u, state: online ? 'online' : 'offline' } : u)
    };
  });
  renderFriends($('searchInput').value);
  renderGroups($('searchInput').value);
  if (Number(activeFriendId) === id) {
    $('topbarStatus').textContent = online ? 'Active now' : 'offline';
  }
  if (activeGroupId != null && !$('infoDrawer').classList.contains('hidden')) {
    renderMemberDrawer();
  }
}

function normalizeGroup(g) {
  if (!g) return null;
  const users = Array.isArray(g.users) ? g.users : [];
  return {
    ...g,
    users,
    member_count: g.member_count != null ? g.member_count : users.length
  };
}

function renderFriends(filter = '') {
  const q = filter.trim().toLowerCase();
  const list = friends.filter((f) => !q || String(f.name).toLowerCase().includes(q) || String(f.id).includes(q));
  $('friendCount').textContent = String(friends.length);
  const el = $('contactList');
  el.innerHTML = '';
  if (!list.length) {
    el.innerHTML = '<li class="empty-hint">暂无好友</li>';
    return;
  }
  list.forEach((f) => {
    const on = f.state === 'online';
    const li = document.createElement('li');
    li.className = 'contact-item' + (Number(f.id) === Number(activeFriendId) ? ' active' : '');
    li.innerHTML = `
      <div class="avatar" style="background:${colorOf(f.id)};background-image:url(${f.avatar || avatarUrl(f.id)});background-size:cover">
        <span class="status-dot ${on ? 'online' : 'offline'}"></span>
      </div>
      <div class="contact-meta">
        <div class="contact-top"><span class="contact-name">${escapeHtml(f.name || '?')}</span>
          <span class="contact-time">#${f.id}</span></div>
        <div class="contact-preview">${on ? '在线' : '离线'}</div>
      </div>`;
    li.onclick = () => openFriendChat(f);
    el.appendChild(li);
  });
}

function renderGroups(filter = '') {
  const q = filter.trim().toLowerCase();
  const list = groups.filter((g) => {
    const name = g.groupname || g.name || '';
    return !q || String(name).toLowerCase().includes(q) || String(g.id).includes(q);
  });
  $('groupCount').textContent = String(groups.length);
  const el = $('groupList');
  el.innerHTML = '';
  if (!list.length) {
    el.innerHTML = '<li class="empty-hint">暂无群聊</li>';
    return;
  }
  list.forEach((g) => {
    const li = document.createElement('li');
    li.className = 'contact-item' + (Number(g.id) === Number(activeGroupId) ? ' active' : '');
    const name = g.groupname || g.name || 'group';
    const n = g.member_count || (g.users && g.users.length) || 0;
    const av = g.avatar || groupAvatarUrl(g.id);
    li.innerHTML = `
      <div class="avatar" style="background:${colorOf(g.id)};background-image:url(${av});background-size:cover"></div>
      <div class="contact-meta">
        <div class="contact-top"><span class="contact-name">${escapeHtml(name)}</span>
          <span class="contact-time">#${g.id}</span></div>
        <div class="contact-preview">${n} 位成员</div>
      </div>`;
    li.onclick = () => openGroupChat(g);
    el.appendChild(li);
  });
}

function escapeHtml(s) {
  return String(s)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}

function updateChatMoreMenu() {
  const isGroup = activeGroupId != null;
  const isFriend = activeFriendId != null;
  $('menuViewMembers').classList.toggle('hidden', !isGroup);
  $('menuQuitGroup').classList.toggle('hidden', !isGroup);
  $('menuDelFriend').classList.toggle('hidden', !isFriend);
}

function openFriendChat(f) {
  closeDrawer();
  closeMenus();
  activeFriendId = f.id;
  activeGroupId = null;
  $('emptyState').classList.add('hidden');
  $('chatWindow').classList.remove('hidden');
  $('topbarName').textContent = f.name || `User ${f.id}`;
  $('topbarStatus').textContent = f.state === 'online' ? 'Active now' : 'offline';
  $('topbarAvatar').innerHTML =
    `<div class="avatar" style="background:${colorOf(f.id)};background-image:url(${f.avatar || avatarUrl(f.id)});background-size:cover"></div>`;
  updateChatMoreMenu();
  renderMessages();
  renderFriends($('searchInput').value);
  renderGroups($('searchInput').value);
  requestHistory(f.id);
}

function openGroupChat(g) {
  closeMenus();
  activeGroupId = g.id;
  activeFriendId = null;
  $('emptyState').classList.add('hidden');
  $('chatWindow').classList.remove('hidden');
  const name = g.groupname || g.name || `Group ${g.id}`;
  const n = g.member_count || (g.users && g.users.length) || 0;
  $('topbarName').textContent = name;
  $('topbarStatus').textContent = `${n} 位成员 · 点击查看`;
  const av = g.avatar || groupAvatarUrl(g.id);
  $('topbarAvatar').innerHTML =
    `<div class="avatar" style="background:${colorOf(g.id)};background-image:url(${av});background-size:cover"></div>`;
  updateChatMoreMenu();
  renderMessages();
  renderFriends($('searchInput').value);
  renderGroups($('searchInput').value);
  if (!$('infoDrawer').classList.contains('hidden')) renderMemberDrawer();
  requestGroupHistory(g.id);
}

function activeStoreKey() {
  if (activeGroupId != null) return chatKey('group', activeGroupId);
  if (activeFriendId != null) return chatKey('friend', activeFriendId);
  return null;
}

function renderMessages() {
  const area = $('messagesArea');
  area.innerHTML = '';
  const key = activeStoreKey();
  const msgs = (key && chatStore[key]) || [];
  const isGroup = key && key.startsWith('g:');
  msgs.forEach((m) => {
    const row = document.createElement('div');
    row.className = 'msg-row ' + (m.sent ? 'sent' : 'recv');
    let avatarHtml = '';
    let nameHtml = '';
    if (isGroup && !m.sent) {
      const av = m.fromAvatar || avatarUrl(m.fromId || 0);
      const nm = m.fromName || resolveName(m.fromId);
      avatarHtml = `<div class="msg-avatar" style="background:${colorOf(m.fromId || 0)};background-image:url(${av});background-size:cover"></div>`;
      nameHtml = `<div class="msg-sender">${escapeHtml(nm)}</div>`;
    }
    row.innerHTML = `${avatarHtml}<div class="msg-bubble-wrap">${nameHtml}<div class="msg-bubble"></div>
      <div class="msg-meta"><span>${m.time || ''}</span></div></div>`;
    row.querySelector('.msg-bubble').textContent = m.text;
    area.appendChild(row);
  });
  area.scrollTop = area.scrollHeight;
}

function bindOptimistic(key, text, msgId) {
  const list = chatStore[key];
  if (!list || msgId == null) return false;
  for (let i = list.length - 1; i >= 0; i--) {
    if (list[i].sent && list[i].msg_id == null && list[i].text === text) {
      list[i].msg_id = msgId;
      if (key === activeStoreKey()) renderMessages();
      return true;
    }
  }
  return false;
}
function pushToStore(key, text, sent, msgId, time, meta) {
  if (!chatStore[key]) chatStore[key] = [];
  if (msgId != null && chatStore[key].some((m) => Number(m.msg_id) === Number(msgId))) return;
  if (sent && msgId != null && bindOptimistic(key, text, msgId)) return;
  chatStore[key].push({
    text,
    sent,
    time: nowLabel(time),
    msg_id: msgId,
    fromId: meta && meta.fromId,
    fromName: meta && meta.fromName,
    fromAvatar: meta && meta.fromAvatar
  });
  if (key === activeStoreKey()) renderMessages();
}
function pushFriendMsg(friendId, text, sent, msgId, time) {
  pushToStore(chatKey('friend', friendId), text, sent, msgId, time);
}
function pushGroupMsg(groupId, text, sent, msgId, time, meta) {
  pushToStore(chatKey('group', groupId), text, sent, msgId, time, meta);
}

function ackOffline(ids) {
  if (!ws || !me || !ids.length) return;
  ws.send(JSON.stringify({ msgid: OFFLINE_ACK, id: me.id, msg_ids: ids }));
}
function requestHistory(peerid) {
  if (!ws || !me || !peerid) return;
  ws.send(JSON.stringify({ msgid: LOAD_HISTORY, id: me.id, peerid: Number(peerid), limit: 50 }));
}
function requestGroupHistory(groupid) {
  if (!ws || !me || !groupid) return;
  ws.send(JSON.stringify({ msgid: LOAD_GROUP_HISTORY, id: me.id, groupid: Number(groupid), limit: 50 }));
}
function extractText(body) {
  if (body == null) return '';
  if (typeof body === 'string') {
    try { body = JSON.parse(body); } catch (e) { return body; }
  }
  return body.msg || body.message || '';
}

function openDrawer() {
  $('infoDrawer').classList.remove('hidden');
  $('infoDrawer').setAttribute('aria-hidden', 'false');
  $('chatMain').classList.add('drawer-open');
  renderMemberDrawer();
}
function closeDrawer() {
  $('infoDrawer').classList.add('hidden');
  $('infoDrawer').setAttribute('aria-hidden', 'true');
  $('chatMain').classList.remove('drawer-open');
}
function renderMemberDrawer() {
  const g = groups.find((x) => Number(x.id) === Number(activeGroupId));
  if (!g) {
    $('drawerTitle').textContent = '群成员';
    $('drawerSub').textContent = '';
    $('memberList').innerHTML = '<li class="empty-hint">无数据</li>';
    return;
  }
  const name = g.groupname || g.name || `Group ${g.id}`;
  const users = Array.isArray(g.users) ? g.users.slice() : [];
  users.sort((a, b) => {
    const ra = a.role === 'creator' ? 0 : 1;
    const rb = b.role === 'creator' ? 0 : 1;
    if (ra !== rb) return ra - rb;
    return String(a.name || '').localeCompare(String(b.name || ''));
  });
  $('drawerTitle').textContent = '群成员';
  $('drawerSub').textContent = `${name} · ${users.length} 人`;
  const el = $('memberList');
  el.innerHTML = '';
  if (!users.length) {
    el.innerHTML = '<li class="empty-hint">暂无成员信息</li>';
    return;
  }
  users.forEach((u) => {
    const on = u.state === 'online';
    const role = u.role === 'creator' ? '群主' : '成员';
    const li = document.createElement('li');
    li.className = 'member-item';
    li.innerHTML = `
      <div class="avatar" style="background:${colorOf(u.id)};background-image:url(${u.avatar || avatarUrl(u.id)});background-size:cover">
        <span class="status-dot ${on ? 'online' : 'offline'}"></span>
      </div>
      <div class="member-meta">
        <div class="member-name">${escapeHtml(u.name || ('#' + u.id))}</div>
        <div class="member-role">${role} · #${u.id} · ${on ? '在线' : '离线'}</div>
      </div>`;
    el.appendChild(li);
  });
}

function openModal(action) {
  closeMenus();
  modalAction = action;
  const body = $('modalBody');
  const title = $('modalTitle');
  if (action === 'add-friend') {
    title.textContent = '添加好友';
    body.innerHTML = `
      <div><label>好友用户 ID</label><input id="modalField1" type="number" min="1" placeholder="例如 2" /></div>
      <p class="hint">对方同意后会出现在好友列表（当前演示为直接双向添加）。</p>`;
  } else if (action === 'create-group') {
    title.textContent = '创建群聊';
    body.innerHTML = `
      <div><label>群名称</label><input id="modalField1" maxlength="32" placeholder="例如 周末局" /></div>
      <div><label>群简介（可选）</label><input id="modalField2" maxlength="64" placeholder="一句话介绍" /></div>`;
  } else if (action === 'join-group') {
    title.textContent = '加入群聊';
    body.innerHTML = `
      <div><label>群 ID</label><input id="modalField1" type="number" min="1" placeholder="例如 1" /></div>
      <p class="hint">向群主或成员索取群 ID 后加入。</p>`;
  } else {
    return;
  }
  $('modalBackdrop').classList.remove('hidden');
  setTimeout(() => {
    const inp = $('modalField1');
    if (inp) inp.focus();
  }, 30);
}
function closeModal() {
  $('modalBackdrop').classList.add('hidden');
  modalAction = null;
  $('modalBody').innerHTML = '';
}
function submitModal() {
  if (!ws || !me || !modalAction) return;
  const v1 = ($('modalField1') && $('modalField1').value || '').trim();
  const v2 = ($('modalField2') && $('modalField2').value || '').trim();
  if (modalAction === 'add-friend') {
    const fid = Number(v1);
    if (!fid) { toast('请输入有效用户 ID'); return; }
    ws.send(JSON.stringify({ msgid: ADD_FRIEND, id: me.id, friendid: fid }));
    toast('已发送加好友请求');
  } else if (modalAction === 'create-group') {
    if (!v1) { toast('请填写群名称'); return; }
    ws.send(JSON.stringify({ msgid: CREATE_GROUP, id: me.id, groupname: v1, groupdesc: v2 }));
    toast('正在创建群聊…');
  } else if (modalAction === 'join-group') {
    const gid = Number(v1);
    if (!gid) { toast('请输入有效群 ID'); return; }
    ws.send(JSON.stringify({ msgid: ADD_GROUP, id: me.id, groupid: gid }));
    toast('正在加入群聊…');
  }
  closeModal();
}

function delActiveFriend() {
  if (!ws || !me || activeFriendId == null) return;
  if (!confirm(`确定删除好友 #${activeFriendId}？`)) return;
  ws.send(JSON.stringify({ msgid: DEL_FRIEND, id: me.id, friendid: Number(activeFriendId) }));
  friends = friends.filter((f) => Number(f.id) !== Number(activeFriendId));
  activeFriendId = null;
  closeDrawer();
  $('chatWindow').classList.add('hidden');
  $('emptyState').classList.remove('hidden');
  renderFriends($('searchInput').value);
  toast('已删除好友');
}
function quitActiveGroup() {
  if (!ws || !me || activeGroupId == null) return;
  if (!confirm(`确定退出群聊 #${activeGroupId}？`)) return;
  ws.send(JSON.stringify({ msgid: QUIT_GROUP, id: me.id, groupid: Number(activeGroupId) }));
  groups = groups.filter((g) => Number(g.id) !== Number(activeGroupId));
  activeGroupId = null;
  closeDrawer();
  $('chatWindow').classList.add('hidden');
  $('emptyState').classList.remove('hidden');
  renderGroups($('searchInput').value);
  toast('已退出群聊');
}

async function refreshZk() {
  try {
    const r = await fetch('/api/zk/userservice');
    const j = await r.json();
    const nodes = j.nodes || [];
    const html = nodes.length
      ? nodes.map((n) => `<li>${escapeHtml(n)}</li>`).join('')
      : '<li>（暂无节点）</li>';
    $('zkNodes').innerHTML = html;
    $('zkNodesLive').innerHTML = html;
    if (j.path) $('zkPath').textContent = j.path;
  } catch (e) {
    $('zkNodes').innerHTML = '<li>fetch failed</li>';
  }
}

function handlePacket(js) {
  if (js.type === 'presence') {
    applyPresence(js.userid, !!js.online);
    return;
  }

  if (js.msgid === LOGIN_ACK) {
    if (js.errno === 0) {
      me = { id: js.id, name: js.name, avatar: js.avatar || avatarUrl(js.id) };
      friends = (js.friends || js.friend || []).map(parseItem).filter(Boolean);
      friends.forEach((f) => { if (!f.avatar) f.avatar = avatarUrl(f.id); });
      groups = (js.groups || []).map(parseItem).map(normalizeGroup).filter(Boolean);
      setLoggedIn(true);
      renderFriends();
      renderGroups();
      if (js.zk_nodes) {
        const html = (js.zk_nodes || []).map((n) => `<li>${escapeHtml(n)}</li>`).join('') || '<li>—</li>';
        $('zkNodes').innerHTML = html;
        $('zkNodesLive').innerHTML = html;
      }
      $('loginStatus').textContent = 'login ok';
      const ids = [];
      // 离线未读只做投递提示；点开会话会用 LOAD_*_HISTORY 覆盖，避免与历史重复
      (js.offlinemsg || []).forEach((item) => {
        let body = item.body || item;
        if (typeof body === 'string') {
          try { body = JSON.parse(body); } catch (e) { body = { msg: body }; }
        }
        const mid = item.msg_id || body.msg_id;
        if (mid) ids.push(mid);
        // 群离线：先放进群会话，刷新后再拉历史会按 msg_id 去重
        if (body.msgid === GROUP_CHAT || body.groupid) {
          const from = body.id || body.fromid;
          pushGroupMsg(body.groupid, extractText(body), false, mid, body.time, {
            fromId: from,
            fromName: body.name || resolveName(from),
            fromAvatar: body.avatar || avatarUrl(from)
          });
        } else if (body.id || body.fromid) {
          const from = body.id || body.fromid;
          // 跳过自己发出的回显残留
          if (Number(from) === Number(me.id)) return;
          pushFriendMsg(from, extractText(body), false, mid, body.time);
        }
      });
      ackOffline(ids);
    } else {
      $('loginStatus').textContent = js.errmsg || ('login fail errno=' + js.errno);
    }
    return;
  }

  if (js.msgid === LOAD_HISTORY_ACK) {
    const peer = js.peerid;
    const key = chatKey('friend', peer);
    chatStore[key] = [];
    (js.list || []).forEach((item) => {
      const body = item.body || {};
      // 防御：旧数据里混进的群消息不进私聊
      if (body.groupid || body.msgid === GROUP_CHAT) return;
      const text = extractText(body);
      const from = item.fromid || body.id;
      const sent = Number(from) === Number(me.id);
      pushFriendMsg(peer, text, sent, item.msg_id, item.time || body.time);
    });
    return;
  }

  if (js.msgid === LOAD_GROUP_HISTORY_ACK) {
    const gid = js.groupid;
    const key = chatKey('group', gid);
    chatStore[key] = [];
    (js.list || []).forEach((item) => {
      const body = item.body || {};
      const text = extractText(body);
      const from = item.fromid || body.id;
      const sent = me && Number(from) === Number(me.id);
      pushGroupMsg(gid, text, sent, item.msg_id, item.time || body.time, {
        fromId: from,
        fromName: body.name || resolveName(from),
        fromAvatar: body.avatar || avatarUrl(from)
      });
    });
    return;
  }

  if (js.msgid === GROUP_CHAT) {
    const gid = js.groupid;
    const text = js.msg || js.message || '';
    const sent = me && Number(js.id) === Number(me.id);
    pushGroupMsg(gid, text, sent, js.msg_id, js.time, {
      fromId: js.id,
      fromName: js.name || resolveName(js.id),
      fromAvatar: js.avatar || avatarUrl(js.id)
    });
    return;
  }

  if (js.msgid === ONE_CHAT || ((js.msg != null || js.message != null) && js.id != null && !js.groupid && js.type !== 'system')) {
    const text = js.msg || js.message || '';
    const from = js.id;
    const to = js.to || js.toid;
    if (!me) return;
    if (Number(from) === Number(me.id)) pushFriendMsg(to, text, true, js.msg_id, js.time);
    else {
      pushFriendMsg(from, text, false, js.msg_id, js.time);
      if (js.msg_id) ackOffline([js.msg_id]);
    }
    return;
  }

  if (js.friends || js.friend) {
    friends = (js.friends || js.friend || []).map(parseItem).filter(Boolean);
    friends.forEach((f) => { if (!f.avatar) f.avatar = avatarUrl(f.id); });
    renderFriends($('searchInput').value);
  }
  if (js.groups) {
    groups = (js.groups || []).map(parseItem).map(normalizeGroup).filter(Boolean);
    renderGroups($('searchInput').value);
    if (activeGroupId != null) {
      const g = groups.find((x) => Number(x.id) === Number(activeGroupId));
      if (g) openGroupChat(g);
      else {
        activeGroupId = null;
        closeDrawer();
        $('chatWindow').classList.add('hidden');
        $('emptyState').classList.remove('hidden');
      }
    }
  }
  if (js.type === 'system' && js.msg) {
    toast(js.msg);
  }
}

function connectAndLogin() {
  if (ws && (ws.readyState === 0 || ws.readyState === 1)) {
    try { ws.close(); } catch (e) {}
  }
  $('loginStatus').textContent = 'connecting ' + wsUrl();
  ws = new WebSocket(wsUrl());
  ws.onopen = () => {
    ws.send(JSON.stringify({
      msgid: LOGIN,
      id: Number($('uid').value),
      password: $('pwd').value
    }));
  };
  ws.onmessage = (ev) => {
    let js;
    try { js = JSON.parse(ev.data); } catch (e) { return; }
    handlePacket(js);
  };
  ws.onclose = () => {
    setLoggedIn(false);
    $('loginStatus').textContent = 'disconnected';
  };
  ws.onerror = () => { $('loginStatus').textContent = 'ws error'; };
}

function onPageLeave() {
  sendLogoutBeacon();
}

function sendChat() {
  if (!ws || !me) return;
  const text = $('msgInput').value.trim();
  if (!text) return;

  if (activeGroupId != null) {
    ws.send(JSON.stringify({
      msgid: GROUP_CHAT,
      id: me.id,
      groupid: Number(activeGroupId),
      msg: text
    }));
    pushGroupMsg(activeGroupId, text, true, null, null, {
      fromId: me.id,
      fromName: me.name,
      fromAvatar: me.avatar || avatarUrl(me.id)
    });
  } else if (activeFriendId != null) {
    ws.send(JSON.stringify({
      msgid: ONE_CHAT,
      id: me.id,
      to: Number(activeFriendId),
      toid: Number(activeFriendId),
      msg: text
    }));
    pushFriendMsg(activeFriendId, text, true);
  } else {
    return;
  }
  $('msgInput').value = '';
}

function logout() {
  if (ws && me) {
    try { ws.send(JSON.stringify({ msgid: LOGINOUT, id: me.id })); } catch (e) {}
    try { ws.close(); } catch (e) {}
  }
  me = null;
  friends = [];
  groups = [];
  activeFriendId = null;
  activeGroupId = null;
  Object.keys(chatStore).forEach((k) => delete chatStore[k]);
  closeDrawer();
  closeModal();
  closeMenus();
  setLoggedIn(false);
  $('chatWindow').classList.add('hidden');
  $('emptyState').classList.remove('hidden');
  renderFriends();
  renderGroups();
}

function bindSectionToggle(btnId, sectionSel) {
  const btn = $(btnId);
  const section = document.querySelector(sectionSel);
  btn.onclick = () => {
    const collapsed = section.classList.toggle('collapsed');
    btn.setAttribute('aria-expanded', collapsed ? 'false' : 'true');
  };
}

function initTheme() {
  // 默认白天模式；换 key 避免旧版强制 dark 的缓存残留
  const saved = localStorage.getItem('nova-ui-theme') || 'light';
  document.documentElement.setAttribute('data-theme', saved);
  $('themeToggle').onclick = () => {
    const cur = document.documentElement.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
    document.documentElement.setAttribute('data-theme', cur);
    localStorage.setItem('nova-ui-theme', cur);
  };
}

function init() {
  initTheme();
  bindSectionToggle('toggleFriends', '.nav-section[data-section="friends"]');
  bindSectionToggle('toggleGroups', '.nav-section[data-section="groups"]');

  $('btnLogin').onclick = connectAndLogin;
  $('btnRefreshZk').onclick = refreshZk;
  $('btnLogout').onclick = logout;
  $('sendBtn').onclick = sendChat;
  $('msgInput').addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      sendChat();
    }
  });
  $('searchInput').addEventListener('input', (e) => {
    renderFriends(e.target.value);
    renderGroups(e.target.value);
  });
  $('backBtn').onclick = () => {
    activeFriendId = null;
    activeGroupId = null;
    closeDrawer();
    $('chatWindow').classList.add('hidden');
    $('emptyState').classList.remove('hidden');
    renderFriends($('searchInput').value);
    renderGroups($('searchInput').value);
  };

  $('btnCompose').onclick = (e) => {
    e.stopPropagation();
    $('chatMoreMenu').classList.add('hidden');
    $('composeMenu').classList.toggle('hidden');
  };
  $('composeMenu').onclick = (e) => {
    const btn = e.target.closest('button[data-action]');
    if (!btn) return;
    openModal(btn.dataset.action);
  };

  $('btnChatMore').onclick = (e) => {
    e.stopPropagation();
    $('composeMenu').classList.add('hidden');
    updateChatMoreMenu();
    $('chatMoreMenu').classList.toggle('hidden');
  };
  $('chatMoreMenu').onclick = (e) => {
    const btn = e.target.closest('button[data-action]');
    if (!btn) return;
    closeMenus();
    const a = btn.dataset.action;
    if (a === 'view-members') openDrawer();
    else if (a === 'del-friend') delActiveFriend();
    else if (a === 'quit-group') quitActiveGroup();
  };

  $('topbarPeerBtn').onclick = () => {
    if (activeGroupId != null) {
      if ($('infoDrawer').classList.contains('hidden')) openDrawer();
      else closeDrawer();
    }
  };
  $('btnCloseDrawer').onclick = closeDrawer;
  $('btnCloseModal').onclick = closeModal;
  $('btnModalCancel').onclick = closeModal;
  $('btnModalOk').onclick = submitModal;
  $('modalBackdrop').addEventListener('click', (e) => {
    if (e.target === $('modalBackdrop')) closeModal();
  });
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      closeModal();
      closeMenus();
    }
  });
  document.addEventListener('click', (e) => {
    if (!e.target.closest('#composeWrap') && !e.target.closest('#chatMoreWrap')) closeMenus();
  });

  $('btnZkToggle').onclick = () => {
    $('zkLivePanel').classList.toggle('hidden');
    if (!$('zkLivePanel').classList.contains('hidden')) refreshZk();
  };

  // 关标签/刷新：先发 LOGINOUT，避免服务端仍显示在线
  window.addEventListener('pagehide', onPageLeave);
  window.addEventListener('beforeunload', onPageLeave);

  refreshZk();
  renderFriends();
  renderGroups();
}

init();
