# NM Two Six Labs C++ Indirect Channel

Implementation of the Two Six Labs Indirect communications utilizing the Two Six Whiteboard.

Channel Survey: https://wiki.race.twosixlabs.com/display/RACE2/Channel+Survey+Template

## Channel Dependencies:

The file channel-dependencies.json is an optional file that should contain any plugins that contain components this channels uses. e.g. If this channel is suppose to test an encoding with the TwoSix transport and usermodel, the channel-dependencies.json file should contain:

```
[
    "PluginCommsTwoSixStubDecomposed"
]
```

The strings are formatted the same as rib arguments. You can specify versions / artifactory env, or a path for local plugin (the path must match the path in rib, not on host).
