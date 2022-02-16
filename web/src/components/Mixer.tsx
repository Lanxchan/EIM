import './Mixer.less'
import React, { useEffect, useState } from 'react'
import useGlobalData, { TrackInfo, DispatchType, ReducerTypes } from '../reducer'
import { ClientboundPacket } from '../Client'
import { Slider, IconButton, Card, Divider, Stack, getLuminance, Button, Tooltip } from '@mui/material'
import Marquee from 'react-fast-marquee'

import Power from '@mui/icons-material/Power'
import VolumeUp from '@mui/icons-material/VolumeUp'
import VolumeOff from '@mui/icons-material/VolumeOff'
import LoadingButton from '@mui/lab/LoadingButton'

interface MixerPlugin { name: string }

interface TrackMixerInfo {
  pan: number
  panRule: number
  plugins: MixerPlugin[]
}

const TrackPlugin: React.FC<{ plugin: MixerPlugin, track: number, index: number }> = ({ plugin, track, index }) => {
  return (
    <>
      <Tooltip title='233' arrow placement='right'>
        <Button
          fullWidth
          size='small'
          color='inherit'
          onClick={() => $client.openPluginWindow(track, index)}
        >
          <Marquee gradient={false} pauseOnHover>{plugin.name}&nbsp;&nbsp;&nbsp;&nbsp;</Marquee>
        </Button>
      </Tooltip>
      <Divider variant='middle' />
    </>
  )
}

const defaultPan = [0, 0]

const Track: React.FC<{ info: TrackInfo, active: boolean, index: number, mixerInfo: TrackMixerInfo, dispatch: DispatchType }> = ({ info, active, index, mixerInfo, dispatch }) => {
  const [pan, setPan] = useState(defaultPan)
  const [loading, setLoading] = useState(false)

  const isMaster = info.uuid === '0'
  useEffect(() => {
    setPan(old => (old[0] === 0 ? old[1] : old[0]) === mixerInfo.pan ? old : mixerInfo.pan > 0 ? [0, mixerInfo.pan] : [mixerInfo.pan, 0])
  }, [mixerInfo.pan])

  return (
    <Card className='track' sx={active ? { border: theme => 'solid 1px ' + theme.palette.primary.main } : undefined}>
      <Button
        fullWidth
        disableElevation
        className='name'
        variant='contained'
        color={isMaster ? 'primary' : undefined}
        style={isMaster ? undefined : { backgroundColor: info.color, color: getLuminance(info.color) > 0.5 ? '#000' : '#fff' }}
        onClick={() => $client.openPluginWindow(index)}
        onDragOver={e => !loading && $dragObject?.type === 'loadPlugin' && $dragObject.isInstrument && e.preventDefault()}
        onDrop={() => {
          if (loading || !$dragObject?.isInstrument || $dragObject.type !== 'loadPlugin') return
          setLoading(true)
          $client.loadPlugin(index, $dragObject.data).finally(() => setLoading(false))
        }}
      >
        <Marquee gradient={false} pauseOnHover>{isMaster ? '主轨道' : info.name}&nbsp;&nbsp;&nbsp;&nbsp;</Marquee>{info.hasInstrument && <Power fontSize='small' />}
      </Button>
      <div className='content'>
        <Slider
          size='small'
          value={pan}
          min={-100}
          sx={{ [`& .MuiSlider-thumb[data-index="${pan[0] === 0 ? 0 : 1}"]`]: { opacity: 0 } }}
          onChange={(_, val) => {
            const [a, b] = val as number[]
            const cur = a === 0 ? b : a
            setPan(cur > 0 ? [0, cur] : [cur, 0])
          }}
        />
        <div className='level'>
          <IconButton size='small' className='mute' onClick={() => $client.updateTrackInfo(index, undefined, undefined, -1, !info.muted, info.solo)}>
            {info.muted ? <VolumeOff fontSize='small' /> : <VolumeUp fontSize='small' />}
          </IconButton>
          <Slider
            size='small'
            orientation='vertical'
            valueLabelDisplay='auto'
            value={Math.sqrt(info.volume) * 100}
            max={140}
            min={0}
            valueLabelFormat={val => `${Math.round(val)}% ${val > 0 ? (Math.log10((val / 100) ** 2) * 20).toFixed(2) + '分贝' : '静音'}`}
            onChange={(_, val) => {
              const volume = ((val as number) / 100) ** 2
              $client.updateTrackInfo(index, undefined, undefined, volume, info.muted, info.solo)
              dispatch({ type: ReducerTypes.UpdateTrack, index, volume })
            }}
          />
          <div className='left'><span>-12.0</span><div /></div>
          <div className='right'><div /></div>
        </div>
      </div>
      <Divider className='mid-divider' />
      <div className='plugins'>
        {mixerInfo.plugins.map((it, i) => <TrackPlugin key={i} plugin={it} track={index} index={i} />)}
        <LoadingButton
          size='small'
          loading={loading}
          sx={{ width: '100%', borderRadius: 0 }}
          onDragOver={e => !loading && $dragObject?.type === 'loadPlugin' && !$dragObject.isInstrument && e.preventDefault()}
          onDrop={() => {
            if (loading || $dragObject?.type !== 'loadPlugin' || $dragObject.isInstrument) return
            setLoading(true)
            $client.loadPlugin(index, $dragObject.data).finally(() => setLoading(false))
          }}
        >
          添加插件
        </LoadingButton>
      </div>
    </Card>
  )
}

const Mixer: React.FC = () => {
  const [globalData, dispatch] = useGlobalData()
  const [mixerInfo, setMixerInfo] = useState<Record<string, TrackMixerInfo>>({})

  useEffect(() => {
    $client.on(ClientboundPacket.TrackMixerInfo, buf => {
      let len = buf.readUint8()
      const onlyOne = len === 1
      const map: Record<string, TrackMixerInfo> = { }
      while (len-- > 0) {
        const { plugins } = map[buf.readUint64().toString(32)] = {
          pan: buf.readInt8(),
          panRule: buf.readUint8(),
          plugins: [] as MixerPlugin[]
        }
        for (let size = buf.readUint8(); size-- > 0;) plugins.push({ name: buf.readIString() })
      }
      setMixerInfo(onlyOne ? old => ({ ...old, ...map }) : map)
    })
    $client.getTracksMixerInfo()
    return () => $client.off(ClientboundPacket.TrackMixerInfo)
  }, [])

  const tracks: JSX.Element[] = []
  globalData.tracks.forEach((it, i) => mixerInfo[it.uuid] &&
    tracks.push(<Track key={it.uuid} info={it} active={globalData.activeTrack === it.uuid} index={i} mixerInfo={mixerInfo[it.uuid]} dispatch={dispatch} />))

  return (
    <Stack className='mixer' spacing={1} direction='row'>
      {tracks}
    </Stack>
  )
}

export default Mixer
