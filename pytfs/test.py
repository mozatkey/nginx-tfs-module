#utf-8
import pytfs

print "testing ",pytfs.version()

def main(svr):
    t = pytfs.TfsClient()
    assert t.init(svr), 'init failed'
    data = 'test' * 1024 * 1024
    tfsname = t.put(data)
    assert tfsname, 'put fail'
    assert t.get(tfsname) == data, 'get data not match'
     
    print "case 1 save file %s success" % tfsname

    fd = t.open("", "", pytfs.WRITE)
    assert fd > 0, "open fail %d" % fd
    t.write(fd, data)
    
    tfsname = t.close(fd)
    
    assert tfsname, 'close fail'
    assert t.get(tfsname) == data, 'get data not match'
    print "case 2 write file %s success" % tfsname
    
    fd = t.open(tfsname, "", pytfs.READ)
    d =  t.read(fd, 4 * 1024 * 1024)
    assert d == data, d
    print "case 3 read file %s success" % tfsname
    
if __name__ == '__main__':
    main("127.0.0.1:8108")
    