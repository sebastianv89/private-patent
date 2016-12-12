# split large xml file in individual parts
i = 0
#d = []
with open('ipg161011.xml', 'r') as fi:
    fo_str = ""
    for line in fi:
        if line.startswith('<?xml'):
            #if i >= 1000 and i <= 1004:
            #    d.append(fo_str)
            fo_name = 'ipg161011-{:05}.xml'.format(i)
            with open(fo_name, 'w') as fo:
                fo.write(fo_str)
            i += 1
            fo_str = ""
        fo_str += line

#ds = ''.join(d)
#with open('ipg161011-small.xml', 'w') as fo:
#    fo.write(ds)

# write last xml file
fo_name = 'ipg161011-{:05}.xml'.format(i)
with open(fo_name, 'w') as fo:
    fo.write(fo_str)
