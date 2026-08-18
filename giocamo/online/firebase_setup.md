# Setting up the database for online play

Online play stores the two players' messages in a Firebase Realtime
Database. You make one database, once, for free. There is no server to run
and nothing to keep alive.

## 1. Make the database

1. Open https://console.firebase.google.com and sign in with a Google
   account.
2. Click **Create a Firebase project**, type a name (for example
   `giocamo`), and click through to the end. Turn Google Analytics off, it
   is not needed.
3. In the left menu click **Build → Realtime Database**, then click
   **Create Database**.
4. Pick the region closest to you and click **Next**.
5. Choose **Start in test mode** and click **Enable**.
6. Copy the URL shown at the top of the Data tab. It looks like
   `https://giocamo-default-rtdb.europe-west1.firebasedatabase.app`.

## 2. Put the URL in the code

1. Open [online.cpp](online.cpp).
2. Replace the text in `DEFAULT_DATABASE_URL` with the URL you copied. Do
   not leave a `/` at the end.
3. Rebuild: `sh run.sh` for the desktop build, `sh web.sh gods` for the
   browser build.

To point one build at a different database without editing the code, set
the `FIREBASE_URL` environment variable, or add `?firebase=https://...` to
the page URL in the browser.

## 3. Set the rules

Test mode stops working after 30 days. Replace the rules so the game keeps
working.

1. In the Realtime Database page click the **Rules** tab.
2. Replace everything with this and click **Publish**:

```json
{
  "rules": {
    "rooms": {
      ".read": true,
      ".write": true
    }
  }
}
```

Anyone who knows a room code can read and write that room. That is the
trade for having no accounts and no login: a friend only has to type four
letters. Room codes are random, and a match lasts minutes, so guessing one
in time is very unlikely.

## What it costs

Nothing. The free Spark plan gives 1 GB of storage and 10 GB of downloads
per month, and it never bills you past that — it stops instead. Two players
swapping small JSON messages use a tiny part of that. Firebase does not
pause a free project for being idle, so the game still works after a quiet
month.

## Where the data goes

Every match writes under `/rooms/<code>`. Nothing deletes those rooms.
When storage gets close to 1 GB, open the Data tab, click the three dots
next to `rooms`, and delete it — no match in progress survives that, so do
it when nobody is playing.
