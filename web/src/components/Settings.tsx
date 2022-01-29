import React, { useEffect, useState } from 'react'
import { useSnackbar, SnackbarKey } from 'notistack'
import { Config, ClientboundPacket } from '../Client'
import {
  Dialog, DialogTitle, Tabs, Tab, CircularProgress, IconButton, colors, List, ListItem, ListItemText,
  ListSubheader, ListItemSecondaryAction, DialogActions, Button, Divider
} from '@mui/material'

import Close from '@mui/icons-material/Close'
import Delete from '@mui/icons-material/Delete'

let scanningKey: SnackbarKey = -1
const Settings: React.FC<{ open: boolean, setOpen: (val: boolean) => void }> = ({ open, setOpen }) => {
  const [tab, setTab] = useState(0)
  const [config, setConfig] = useState<Config>()
  const [scanning, setScanning] = useState(false)
  const { enqueueSnackbar, closeSnackbar } = useSnackbar()

  useEffect(() => {
    $client.config().then(it => setConfig(JSON.parse(it)))
  }, [open])

  useEffect(() => {
    $client.on(ClientboundPacket.ScanVSTs, buf => {
      if (buf.readUint8()) {
        setScanning(true)
        scanningKey = enqueueSnackbar('插件扫描中...', { persist: true, variant: 'info' })
      } else {
        setScanning(false)
        closeSnackbar(scanningKey)
      }
    })
  }, [])
  const onClose = () => setOpen(false)

  return (
    <Dialog onClose={onClose} open={open} sx={{ '& .MuiDialog-paper': { minWidth: 180, minHeight: 200 } }}>
      <DialogTitle>
        首选项
        <IconButton
          onClick={onClose}
          sx={{
            position: 'absolute',
            right: 8,
            top: 8,
            color: colors.grey[500]
          }}
        >
          <Close />
        </IconButton>
      </DialogTitle>
      {config
        ? (
          <>
            <div style={{ display: 'flex' }}>
              <Tabs orientation='vertical' value={tab} onChange={(_, val) => setTab(val)} sx={{ borderRight: 1, borderColor: 'divider' }}>
                <Tab label='常规' />
                <Tab label='音频' />
                <Tab label='插件' />
              </Tabs>
              {tab === 2 && (
                <List dense>
                  {Object.entries(config.vstSearchPaths).map(([key, val], i) => (
                    <React.Fragment key={key}>
                      {i ? <Divider /> : null}
                      <ListSubheader>{key}</ListSubheader>
                      {val.map(it => (
                        <ListItem key={it}>
                          <ListItemText primary={it} />
                          <ListItemSecondaryAction>
                            <IconButton edge='end'><Delete /></IconButton>
                          </ListItemSecondaryAction>
                        </ListItem>
                      ))}
                    </React.Fragment>
                  ))}
                </List>
              )}
            </div>
            {tab === 2 && (
              <DialogActions>
                <Button onClick={() => $client.openPluginManager()} color='primary'>打开插件管理器</Button>
                <Button onClick={() => $client.scanVSTs()} color='primary' disabled={scanning}>扫描插件</Button>
              </DialogActions>
            )}
          </>
          )
        : <CircularProgress sx={{ margin: 'auto', position: 'relative', top: '-20px' }} />}
    </Dialog>
  )
}

export default Settings